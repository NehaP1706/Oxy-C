#include "shell.h"

void do_in_bg(int bg_until, ShellCmd* cmd, Job *jobs, int *job_count, int *next_job_id, char* dir_name, char* cwd, char* shell_home, Token* tokens, int* count, int* start, char logs[15][4097], int *parse_error, bool log_enabled, pid_t *fg_pid)
{
    pid_t pid = fork();

    if (pid == 0) {
        // ---------- CHILD ----------
        // IDENTICAL loop you pasted, just using cmd
        setpgid(getpid(), getpid());
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        for (int g = 0; g <= bg_until; g++) {

            if ((cmd->groups[g]).natoms == 1) {
                Atomic *at = &((cmd->groups[g]).atoms[0]);

                if (!at->infile && !at->outfile && at->argv && at->argv[0] &&
                    strcmp(at->argv[0], "hop") == 0) {
                    handle_hop(dir_name, at->argv, shell_home); 
                }
                else if (!at->infile && !at->outfile && at->argv && at->argv[0] &&
                    strcmp(at->argv[0], "reveal") == 0) {
                    printf("\n");
                    int a = 0, l = 0;
                    char* pathname = malloc(1024);
                    assign_pathname(pathname, at, &a, &l, dir_name, shell_home, prev_dir_reveal);
                    handle_reveal(pathname, a, l, prev_dir_reveal);
                    free(pathname);
                }
                else if (!at->infile && !at->outfile && at->argv && at->argv[0] && strcmp(at->argv[0], "log") == 0) {
                printf("\n");

                        //printf("START: %d, COUNT: %d\n", start, count);
                        //int num = get_num(at->argv[2]);
                        //printf("NUM: %d\n", num);

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

                            if (num == -1)
                            {
                                syntax_error("Invalid syntax", parse_error);
                            }
                            else
                            {
                                int idx = (*count - *start) - ((num - 1)%15)%15 - 1;
                                //printf("IDX: %d\n", idx);

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
                else if (!at->infile && !at->outfile && at->argv && at->argv[0]) {
                    int rc = fork();
                    *fg_pid = rc;

                    char fg_cmdline[1024];
                    snprintf(fg_cmdline, sizeof(fg_cmdline), "%s", at->argv[0]);

                    if (rc == 0) {
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
                        waitpid(rc, &status, WUNTRACED);

                        signal(SIGINT, SIG_IGN);
                        signal(SIGTSTP, SIG_IGN);
                        *fg_pid = -1;
                    }
                }
                else {
                    execute_command(tokens, at, count, start, logs,
                                    dir_name, cwd, shell_home,
                                    parse_error, jobs, job_count, next_job_id,
                                    fg_pid, log_enabled);
                }
            }
            else {
                if (cmd->types[0] == T_SEMI) {
                    execute_sequential(cmd, tokens, g, count, start, logs,
                                       cwd, dir_name, shell_home, parse_error,
                                       jobs, job_count, next_job_id, fg_pid, log_enabled);
                } else {
                    execute_pipeline(tokens, cmd, g, count, start, logs,
                                     dir_name, cwd, shell_home, parse_error,
                                     jobs, job_count, next_job_id, fg_pid, log_enabled);
                }
            }
        }

        //printf("PRINT SHIT BROOO");
        exit(0);  // child exits after running bg job
    }
    else {
    // ---------- PARENT ----------
        //printf("PRINT SHIT BROOO\n");
        jobs[*job_count].pid = pid;
        jobs[*job_count].job_id = (*next_job_id)++;
        jobs[*job_count].state = RUNNING;

        // Always clear cmdline
        jobs[*job_count].cmdline[0] = '\0';

        for (int gr = 0; gr < bg_until; gr++)
        {
            for (int a =0; a < cmd->groups[gr].natoms; a++)
            {
                Atomic *at = &cmd->groups[gr].atoms[a];
                for (int i = 0; at->argv && at->argv[i]; i++) {
                    strcat(jobs[*job_count].cmdline, at->argv[i]);
                    strcat(jobs[*job_count].cmdline, " ");
                }
            }
        }

        printf("[%d] %d %s\n", jobs[*job_count].job_id, pid, jobs[*job_count].cmdline);

        (*job_count)++;
        fflush(stdout);
    }
}

void do_fg(int jid, Job* jobs, int *job_count, pid_t *fg_pid) {
    int i;
    Job *job = NULL;

    // If no job id is provided, pick the most recent background/stopped job
    if (jid == -1 && *job_count > 0) {
        job = &jobs[*job_count - 1];
    } else {
        for (i = 0; i < *job_count; i++) {
            if (jobs[i].job_id == jid) {
                job = &jobs[i];
                break;
            }
        }
    }

    if (!job) {
        printf("No such job\n");
        return;
    }

    printf("%s\n", job->cmdline);

    if (job->state == STOPPED) {
        kill(job->pid, SIGCONT);  // Resume the job
        job->state = RUNNING;
    }

    *fg_pid = job->pid;

    // Wait for job to finish or stop
    int status;
    waitpid(job->pid, &status, WUNTRACED);

    // If the job stopped again, update its state
    if (WIFSTOPPED(status)) {
        job->state = STOPPED;
    } else {
        remove_job(jobs, job_count, i);
        // Job finished, remove from job list (optional)
        *fg_pid = -1;
        // could shift jobs array here if you want to remove finished job
    }
}

void do_bg(int jid, Job* jobs, int *job_count) {
    int i;
    Job *job = NULL;

    if (jid == -1 && *job_count > 0) {
        job = &jobs[*job_count - 1];
    } else {
        for (i = 0; i < *job_count; i++) {
            if (jobs[i].job_id == jid) {
                job = &jobs[i];
                break;
            }
        }
    }

    if (!job) {
        printf("No such job\n");
        return;
    }

    if (job->state == RUNNING) {
        printf("Job already running\n");
        return;
    }

    // Resume job in background
    kill(job->pid, SIGCONT);
    job->state = RUNNING;

    printf("[%d] %s &\n", job->job_id, job->cmdline);
}