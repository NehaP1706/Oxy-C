#include "shell.h"

void do_in_bg(CmdGroup g, Job *jobs, int *job_count, int *next_job_id, char* dir_name, char* cwd, char* shell_home)
{
    pid_t pid = fork();

    if (pid == 0) {

        // --------- single atomic -----------
        if (g.natoms == 1) {
            Atomic *at = &g.atoms[0];

            if (at->infile) {
                int fd = open(at->infile, O_RDONLY);
                if (fd < 0) { perror("No such file or directory!"); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            else
            {
                int devnull = open("/dev/null", O_RDONLY);
                dup2(devnull, STDIN_FILENO);
                close(devnull);
            }

            if (at->outfile) {
                int fd;
                if (at->append)
                    fd = open(at->outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
                else
                    fd = open(at->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror("Cannot open output file"); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            if (at->argv && at->argv[0]) {
                if (strcmp(at->argv[0], "hop") == 0) {
                    handle_hop(dir_name, at->argv, shell_home);
                    exit(0);
                }
                else if (strcmp(at->argv[0], "reveal") == 0) {
                    int a = 0, l = 0;
                    char* pathname = malloc(1024);
                    assign_pathname(pathname, at, &a, &l, dir_name, shell_home, prev_dir_reveal);
                    handle_reveal(pathname, a, l, prev_dir_reveal);
                    free(pathname);
                    exit(0);
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
                else {
                    execvp(at->argv[0], at->argv);
                    perror("execvp failed");
                    exit(1);
                }
            }
            exit(0);
        }
    }
    else if (pid > 0) {
        jobs[*job_count].pid = pid;
        jobs[*job_count].job_id = (*next_job_id)++;

        // Build full command line string for logging
        jobs[*job_count].cmdline[0] = '\0';
        for (int i = 0; g.atoms[0].argv && g.atoms[0].argv[i]; i++) {
            strcat(jobs[*job_count].cmdline, g.atoms[0].argv[i]);
            strcat(jobs[*job_count].cmdline, " ");
        }

        jobs[*job_count].state = RUNNING;
        (*job_count)++;

        printf("[%d] %d\n", jobs[*job_count - 1].job_id, pid);
        fflush(stdout);
    }
    else {
        perror("fork failed");
    }
}