#include "shell.h"

int get_num(char* input)
{
	int n = strlen(input);
	int num = 0;
    int i = 0;

	while(i < n)
	{
		if (input[i] <= '9' && input[i] >= '0') 
		{
			num = num*10 + (input[i] - '0'); 
		}
		else
		{
			return -1;
		}

		i++;
	}

	return num;
}

void check_jobs(Job *jobs, int *job_count) {
    int status;
    pid_t result;

    // Reap all finished / stopped / continued children
    while ((result = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        for (int i = 0; i < *job_count; i++) {
            if (jobs[i].pid == result) {
                if (WIFEXITED(status)) {
                    printf("%s with pid %d exited normally\n",
                           jobs[i].cmdline, jobs[i].pid);

                    // Remove from job list
                    if (i < *job_count - 1) {
                        memmove(&jobs[i], &jobs[i+1],
                                (*job_count - i - 1) * sizeof(Job));
                    }
                    (*job_count)--;
                    i--;

                } else if (WIFSIGNALED(status)) {
                    printf("%s with pid %d exited abnormally (signal %d)\n",
                           jobs[i].cmdline, jobs[i].pid, WTERMSIG(status));

                    // Only remove if it actually *terminated*
                    if (i < *job_count - 1) {
                        memmove(&jobs[i], &jobs[i+1],
                                (*job_count - i - 1) * sizeof(Job));
                    }
                    (*job_count)--;
                    i--;

                } else if (WIFSTOPPED(status)) {
                    //printf("%s with pid %d stopped (signal %d)\n",jobs[i].cmdline, jobs[i].pid, WSTOPSIG(status));
                    jobs[i].state = STOPPED;

                } else if (WIFCONTINUED(status)) {
                    //printf("%s with pid %d continued\n", jobs[i].cmdline, jobs[i].pid);
                    jobs[i].state = RUNNING;
                }
            }
        }
    }

    if (result == -1 && errno != ECHILD) {
        perror("waitpid");
    }
}