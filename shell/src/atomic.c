#include "shell.h"

void execute_command(Token* tokens, Atomic* at, int* count, int* start, char logs[15][4097], char* dir_name, char*cwd, char* shell_home, int* parse_error, Job* jobs, int* job_count, int* next_job_id, int* fg_pid, bool log_enabled) {
    pid_t pid = fork();
    
    if (pid == 0) {
        *fg_pid = getpid();
        setpgid(getpid(), getpid());
                            
        signal(SIGINT, sigint_handler);
        signal(SIGTSTP, sigtstp_handler);

        if (at->infile) {
            int fd = open(at->infile, O_RDONLY);
            if (fd < 0) {
                printf("No such file or directory!");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        if (at->outfile) {
            int fd;
            if (at->append)
                fd = open(at->outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
            else
                fd = open(at->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (fd < 0) {
                printf("Unable to create file for writing");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        if (strcmp(at->argv[0], "reveal") == 0)
        {
            int a = 0, l=0;
            char* pathname = (char*) malloc (sizeof(char)*1024);

            assign_pathname(pathname, at, &a, &l, dir_name, shell_home, prev_dir_reveal);
                        
            handle_reveal(pathname, a, l, prev_dir_reveal);
            free(pathname);
            exit(0);
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
                    int idx = ((num - 1)%15)%15;
                    //printf("EXECUTING LINE: %d", idx);
                    fflush(stdout);

                    execute_fn(idx, tokens, count, start, logs, jobs, job_count, next_job_id, dir_name, cwd, shell_home, fg_pid);
                }
            }
            else
            {
                printf("Invalid syntax!\n");
            }
        }
        else if (strcmp(at->argv[0], "ping") == 0) {
            if (at->argv[1] && at->argv[2]) {
                pid_t pid = atoi(at->argv[1]);
                int sig = atoi(at->argv[2]) % 32;

                if (kill(pid, sig) == -1) {
                    printf("Invalid syntax!");
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
        else 
        {
            execvp(at->argv[0], at->argv);
            printf("Command not found!\n");
            exit(1);
        }

    } else {
        *fg_pid = pid;
        setpgid(pid, pid);

        signal(SIGINT, sigint_handler);
        signal(SIGTSTP, sigtstp_handler);

        int status;
        waitpid(pid, &status, WUNTRACED);  // wait for exit or stop

        signal(SIGINT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        *fg_pid = -1;  
    }
}