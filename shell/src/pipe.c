#include "shell.h"

void execute_pipeline(Token* tokens, ShellCmd *cmd, int g, int* count, int* start,
    char logs[15][4097], char* dir_name, char* cwd, char* shell_home,
    int* parse_error, Job* jobs, int* job_count, int* next_job_id,
    int* fg_pid, bool log_enabled) 
{
    int num_atoms = cmd->groups[g].natoms;
    int pipes[num_atoms-1][2];

    // Create pipes
    for (int i = 0; i < num_atoms-1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            exit(1);
        }
    }

    for (int a = 0; a < num_atoms; a++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); exit(1); }

        if (pid == 0) {   // ---- CHILD ----
            *fg_pid = getpid();
            setpgid(getpid(), getpid());

            signal(SIGINT, sigint_handler);
            signal(SIGTSTP, sigtstp_handler);

            Atomic *at = &cmd->groups[g].atoms[a];

            // stdin from previous pipe (if not first)
            if (a > 0) dup2(pipes[a-1][0], STDIN_FILENO);

            // stdout to next pipe (if not last)
            if (a < num_atoms-1) dup2(pipes[a][1], STDOUT_FILENO);

            // --- MULTIPLE INPUT REDIRECTIONS ---
            if (at->ninfiles > 0) {
                int pipefd[2];
                if (pipe(pipefd) == -1) { perror("pipe"); exit(1); }

                pid_t writer = fork();
                if (writer == 0) {
                    close(pipefd[0]); // writer child
                    char buffer[4096];
                    for (int i = 0; i < at->ninfiles; i++) {
                        int fd = open(at->infiles[i], O_RDONLY);
                        if (fd < 0) { printf("No such file or directory\n"); exit(1); }
                        ssize_t n;
                        while ((n = read(fd, buffer, sizeof(buffer))) > 0)
                            write(pipefd[1], buffer, n);
                        close(fd);
                    }
                    close(pipefd[1]);
                    exit(0);
                }
                close(pipefd[1]);  // parent closes write end
                dup2(pipefd[0], STDIN_FILENO);
                close(pipefd[0]);
                int status;
                waitpid(writer, &status, 0);
            }

            // --- MULTIPLE OUTPUT REDIRECTIONS ---
            if (at->noutfiles > 0) {
                for (int i = 0; i < at->noutfiles; i++) {
                    int fd;
                    if (at->append) 
                        fd = open(at->outfiles[i], O_WRONLY | O_CREAT | O_APPEND, 0644);
                    else 
                        fd = open(at->outfiles[i], O_WRONLY | O_CREAT | O_TRUNC, 0644);

                    if (fd < 0) { printf("Unable to create file for writing\n"); exit(1); }
                    dup2(fd, STDOUT_FILENO); 
                    close(fd);
                }
            }

            //close all pipes
            for (int i = 0; i < num_atoms-1; i++) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }

            // --- builtins & exec ---
            if (at->argv && at->argv[0]) {
                if (strcmp(at->argv[0], "hop") == 0) {
                    handle_hop(dir_name, at->argv, shell_home);
                } 
                else if (strcmp(at->argv[0], "reveal") == 0) {
                    int a = 0, l = 0;
                    char* pathname = malloc(1024);
                    assign_pathname(pathname, at, &a, &l, dir_name, shell_home, prev_dir_reveal);
                    handle_reveal(pathname, a, l, prev_dir_reveal);
                    free(pathname);
                }
                else if (log_enabled && at->argv[0] && strcmp(at->argv[0], "log") == 0) {
                    //printf("\n");

                    if (at->argv[1] == NULL)
                    {
                        for (int i=0; i<*count; i++)
                        {
                            int idx = (*start + i)%15;
                            printf("%s\n", logs[idx]);
                        }
                    }
                    else if (strcmp(at->argv[1], "purge") == 0)
                    {
                        *start = 0;
                        *count = 0;
                    } 
                    else if (strcmp(at->argv[1], "execute") == 0 && at->argv[2])
                    {
                        //printf("%s %s %s\n", at->argv[0], at->argv[1], at->argv[2]);
                        int num = get_num(at->argv[2]);
                        //printf("%d %d\n", *count, *start);
                        int idx = (*count - *start) - ((num - 1) % 15) % 15 - 1;
                        //printf("%d %d\n", num, idx);    
                        char* replay = get_in(idx);

        if (!replay) {
            printf("eeefInvalid syntax!\n");
            exit(1);
        }

        // Strip newline
        size_t L = strlen(replay);
        if (L > 0 && replay[L-1] == '\n') replay[L-1] = '\0';

        pid_t rpid = fork();
        if (rpid < 0) {
            perror("fork");
            exit(1);
        }
        if (rpid == 0) {
            // replay child inherits stdin/stdout of pipeline child
            execute_string(
                replay,
                tokens,
                count, start,
                logs,
                jobs, job_count, next_job_id,
                dir_name, cwd, shell_home,
                fg_pid,
                true   // piped context
            );
            _exit(0); // safety
        } else {
            int st;
            waitpid(rpid, &st, 0);
            free(replay);
        }
        
                    }
                    else
                    {
                        printf("Invalid Syntax!\n");
                    }
                }
                else if (strcmp(at->argv[0], "fg") == 0) {
                    int jid = at->argv[1] ? atoi(at->argv[1]) : -1;
                    do_fg(jid, jobs, job_count, fg_pid);
                }
                else if (strcmp(at->argv[0], "bg") == 0) {
                    int jid = at->argv[1] ? atoi(at->argv[1]) : -1;
                    do_bg(jid, jobs, job_count);
                }
                else if (strcmp(at->argv[0], "ping") == 0) {
                    if (at->argv[1] && at->argv[2]) {
                        pid_t pid = atoi(at->argv[1]);
                        int sig = atoi(at->argv[2]) % 32;
                        if (kill(pid, sig) == -1) printf("Invalid syntax!\n");
                        else printf("Sent signal %d to process with pid %d\n", sig, pid);
                    } else printf("Invalid syntax!\n");
                }
                else if (at->argv[0] && strcmp(at->argv[0], "activities") == 0) {
                    for (int i = 0; i < *job_count - 1; i++) {
                        for (int j = i + 1; j < *job_count; j++) {
                            if (strcmp(jobs[i].cmdline, jobs[j].cmdline) > 0) {
                                Job tmp = jobs[i];
                                jobs[i] = jobs[j];
                                jobs[j] = tmp;
                            }
                        }
                    }

                    for (int i = 0; i < *job_count; i++) {
                        printf("[%d] : %s - %s\n", 
                        jobs[i].pid,
                        jobs[i].cmdline,
                        jobs[i].state == RUNNING ? "Running" : "Stopped");
                    }
                }
                else {
                    execvp(at->argv[0], at->argv);
                    printf("Command not found!\n");
                    exit(1);
                }
            }
            exit(0);
        }
    }

    // parent closes pipes
    for (int i = 0; i < num_atoms-1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // wait for all children
    for (int a = 0; a < num_atoms; a++) {
        wait(NULL);
    }
}