#include "shell.h"

void execute_pipeline(Token* tokens, ShellCmd *cmd, int g, int* count, int* start, char logs[15][4097], char* dir_name, char* cwd, char* shell_home, int* parse_error, Job* jobs, int* job_count, int* next_job_id, int* fg_pid, bool log_enabled) {
    int num_atoms = cmd->groups[g].natoms;
    int pipes[num_atoms-1][2];

    // Create pipes
    for (int i=0; i<num_atoms-1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe failed");
            exit(1);
        }
    }

    for (int a=0; a<num_atoms; a++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(1);
        }
        if (pid == 0) {
            fg_pid = getpid();
            setpgid(getpid(), getpid());
                            
            signal(SIGINT, sigint_handler);
            signal(SIGTSTP, sigtstp_handler);

            Atomic *at = &cmd->groups[g].atoms[a];

            // stdin from previous pipe (if not first)
            if (a > 0) {
                dup2(pipes[a-1][0], STDIN_FILENO);
            }

            // stdout to next pipe (if not last)
            if (a < num_atoms-1) {
                dup2(pipes[a][1], STDOUT_FILENO);
            }

            // Apply file redirection
            if (at->infile && a == 0) {
                int fd = open(at->infile, O_RDONLY);
                if (fd < 0) { perror("open infile"); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (at->outfile && a == num_atoms-1) {
                int flags = O_WRONLY | O_CREAT;
                if (at->append) {
                    flags |= O_APPEND;
                } else {
                    flags |= O_TRUNC;
                }
                int fd = open(at->outfile, flags, 0644);
                if (fd < 0) { perror("open outfile"); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            for (int i=0; i<num_atoms-1; i++) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }

            if (strcmp(at->argv[0], "hop") == 0)
            {
                handle_hop(dir_name, at->argv, shell_home); 
            }
            else if (strcmp(at->argv[0], "reveal") == 0)
            {
                int a = 0, l=0;
                char* pathname = (char*) malloc (sizeof(char)*1024);

                assign_pathname(pathname, at, &a, &l, dir_name, shell_home);
                        
                handle_reveal(pathname, a, l);
                free(pathname);
            }
            else if (log_enabled && at->argv[0] && strcmp(at->argv[0], "log") == 0) {
                printf("\n");

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
                    int num = get_num(at->argv[2]);
                    printf("NUM: %d\n", num);

                    if (num == -1)
                    {
                        syntax_error("Invalid syntax", parse_error);
                    }
                    else
                    {
                        int idx = ((*count - *start)%15 + (num - 1)%15)%15;
                        printf("EXECUTING LINE: %d", idx);
                        fflush(stdout);

                        execute_fn(logs[idx], tokens, count, start, logs, jobs, job_count, next_job_id, dir_name, cwd, shell_home, fg_pid);
                    }
                }
            }
            else if (strcmp(at->argv[0], "ping") == 0) {
                if (at->argv[1] && at->argv[2]) {
                    pid_t pid = atoi(at->argv[1]);
                    int sig = atoi(at->argv[2]) % 32;

                    if (kill(pid, sig) == -1) {
                        perror("No such process found");
                    } else {
                        printf("Sent signal %d to process with pid %d\n", sig, pid);
                    }
                } else {
                    printf("Usage: ping <pid> <signal_number>\n");
                }
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
            else if (at->argv && at->argv[0])
            {
                printf("CALLING EXEC 2\n");
                execvp(at->argv[0], at->argv);
                printf("EXEC() ERROR.");
            }
            else 
            {
                printf("CALLING EXEC 1\n");
                execvp(at->argv[0], at->argv);
                perror("execvp failed");
                exit(1);
            }
        }
    }

    for (int i=0; i<num_atoms-1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    for (int a=0; a<num_atoms; a++) {
        wait(NULL);
    }
}

