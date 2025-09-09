#include "shell.h"

void execute_sequential(ShellCmd *cmd, Token* tokens, int g, int* count, int* start, char logs[15][4097], char* cwd, char* dir_name, char* shell_home, int* parse_error, Job* jobs, int* job_count, int* next_job_id, int* fg_pid, bool log_enabled) {
    for (int i = 0; i < cmd->ngroups; i++) {
        if (cmd->groups[g].natoms == 1) {
            Atomic *at = &cmd->groups[g].atoms[0];

            if (at->argv[0] && strcmp(at->argv[0], "hop") == 0) {
                handle_hop(dir_name, at->argv, shell_home);  
                printf("NEW DIR_NAME: %s\n", dir_name);            
            }
            else if (at->argv[0] && strcmp(at->argv[0], "reveal") == 0) {
                printf("\n");
                int a = 0, l=0;
                char* pathname = (char*) malloc (sizeof(char)*1024);

                assign_pathname(pathname, at, &a, &l, dir_name, shell_home, prev_dir_reveal);
                        
                handle_reveal(pathname, a, l, prev_dir_reveal);              
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
                        int idx = (*count - *start) - ((num - 1)%15)%15 - 1;
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
                        printf("Invalid syntax!\n");
                    } else {
                        printf("Sent signal %d to process with pid %d\n", sig, pid);
                    }
                } else {
                    printf("Invalid syntax!\n");
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
                int rc = fork();
                printf("started %s\n", at->argv[0]);

                if (rc == 0) {
                // child
                    *fg_pid = getpid();
                    setpgid(getpid(), getpid());
                
                    signal(SIGINT, sigint_handler);
                    signal(SIGTSTP, sigtstp_handler);

                    execvp(at->argv[0], at->argv);
                    printf("Command not found!\n");
                    exit(1);
                } else {
                    *fg_pid = rc;
                    setpgid(rc, rc);

                    signal(SIGINT, sigint_handler);
                    signal(SIGTSTP, sigtstp_handler);

                    int status;
                    printf("ended %s", at->argv[0]);
                    waitpid(rc, &status, WUNTRACED);  // wait for exit or stop

                    signal(SIGINT, SIG_IGN);
                    signal(SIGTSTP, SIG_IGN);
                    *fg_pid = -1;                        
                }
            }
            else
            {
                //printf("CALLING SINGLE INPUT OUTPUT REDIRECTION\n");
                execute_command(tokens, at, count, start, logs, dir_name, cwd, shell_home, parse_error, jobs, job_count, next_job_id, fg_pid, log_enabled);
                //exit(0);
            }
        }
        else
        {
            execute_pipeline(tokens, cmd, g, count, start, logs, dir_name, cwd, shell_home, parse_error, jobs, job_count, next_job_id, fg_pid, log_enabled);
        }
    }
}

