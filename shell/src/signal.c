#include "shell.h"

/* Handle Ctrl-C */
void sigint_handler(int sig) {
    if (fg_pid == shell_pgid)
    {
        return;
    }
    if (fg_pid > 0) {
        // Send SIGINT to the entire foreground process group
        //printf("in %d sent\n", fg_pid);
        //printf("in fg_pid: %d\n", fg_pid);
        kill(-fg_pid, SIGINT);
    }
    // Shell itself should not exit
    //printf("%d sent\n", shell_pgid);
    //printf("fg_pid: %d\n", fg_pid);
}

/* Handle Ctrl-Z */
void sigtstp_handler(int sig) {
    if (fg_pid == shell_pgid)
    {
        return;
    }
    if (fg_pid > 0) {
        // Stop the foreground process group
        printf("%d\n", fg_pid);
        kill(-fg_pid, SIGTSTP);

        // Save it in jobs[]
        if (job_count < 64) {
            jobs[job_count].pid = fg_pid;
            jobs[job_count].job_id = next_job_id++;
            jobs[job_count].state = STOPPED;

            strncpy(jobs[job_count].cmdline, fg_cmdline,
                    sizeof(jobs[job_count].cmdline) - 1);
            jobs[job_count].cmdline[sizeof(jobs[job_count].cmdline) - 1] = '\0';

            printf("[%d] Stopped %s\n",
                   jobs[job_count].job_id,
                   jobs[job_count].cmdline);
            fflush(stdout);

            job_count++;
        }

        fg_pid = -1; // mark no fg job
    }
}

/* Install Ctrl-C and Ctrl-Z handlers */
void install_handlers() {
    struct sigaction sa;

    // SIGINT (Ctrl-C)
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; 
    sigaction(SIGINT, &sa, NULL);

    // SIGTSTP (Ctrl-Z)
    sa.sa_handler = sigtstp_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa, NULL);
}

/* Handle Ctrl-D (EOF) inside main loop */
void handle_eof() {
    // Kill all background jobs
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].state == RUNNING || jobs[i].state == STOPPED) {
            kill(-jobs[i].pid, SIGKILL);
        }
    }

    printf("logout\n");
    fflush(stdout);
    exit(0);
}

