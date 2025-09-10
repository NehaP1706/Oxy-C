#include "shell.h"

void add_log(char* cmd, char logs[15][4097], int* start, int* count) {
    int n = strlen(cmd);

    if (n >= 3 && cmd[0] == 'l' && cmd[1] == 'o' && cmd[2] == 'g')
    {
        return;
    }
    
    if ((*count) > 0) {
        int last_index = (*start + *count - 1) % 15;
        if (strcmp(logs[last_index], cmd) == 0) return;
    }

    if (*count < 15) {
        strcpy(logs[(*start + *count) % 15], cmd);
        //printf("Just updated index1: %d\n", (*start + *count)%15);
        (*count)++;
    } else {
        strcpy(logs[*start], cmd);
        //printf("Just updated index2: %d\n", (*start));
        *start = (*start + 1) % 15;
    }
}

void test_state(int* start, int* count, char logs[15][4097])
{
    printf("CURRENT STATE OF LOGS.TXT: \n");

    for (int i= 0; i< *count; i++)
    {
        int idx = (*start + i)%15;
        printf("%s\n", logs[idx]);
    }
}

void update_logs(int* start, int* count, char logs[15][4097], char* shell_home)
{
    char* file_name = (char*) malloc (sizeof(char)*4097);
    strcpy(file_name, shell_home);
    strcat(file_name, "/logs.txt");

    FILE* fptr;
    fptr = fopen(file_name, "w");

    for (int i = 0; i < *count; i++) {
        int idx = (*start + i)%15;
        fprintf(fptr, "%s\n", logs[idx]);
    }

    fclose(fptr);
    free(file_name);
}

char* get_in(int idx)
{
    FILE* fptr = fopen("logs.txt", "r");
    if (!fptr) {
        perror("No such file or directory");
        return NULL;
    }

    char buffer[4097];
    int i = 0;

    while (fgets(buffer, sizeof(buffer), fptr)) {
        if (i == idx) {
            fclose(fptr);

            // allocate exact space and copy
            char* result = malloc(strlen(buffer) + 1);
            if (result) {
                strcpy(result, buffer);
            }
            return result;
        }
        i++;
    }

    fclose(fptr);
    return NULL; // idx out of range
}

void execute_fn(int idx, Token* tokens, int* count, int* start, char logs[15][4097], Job* jobs, int* job_count, int* next_job_id, char* dir_name, char* cwd, char* shell_home, int* fg_pid)
{
    char* input = get_in(idx);
    
    int pos = 0;
    int parse_error = 0;
    int ntok = 0;

    //printf("GOTTA TOKENIZE: %s\n", input);

    tokenize(input, tokens, &ntok);
    //char cmds[4097] = "";

    //inspect_tokens(tokens, &ntok, cmds, 4097);

    ShellCmd cmd = parse_shell_cmd(tokens, &pos, &parse_error);

    if (!parse_error && peek(tokens,&pos)->type == T_EOF) {
        //printf("Parsed %d group(s)\n", cmd.ngroups);

        if (cmd.trailing_amp == 1 && cmd.bg_until != -1) {
            do_in_bg(cmd.bg_until, &cmd, jobs, job_count, next_job_id, dir_name, cwd, shell_home, tokens, count, start, logs, &parse_error, true, fg_pid);
        }

        for (int g=cmd.bg_until + 1; g<cmd.ngroups; g++) {

            if (cmd.groups[g].natoms == 1) {
                Atomic *at = &cmd.groups[g].atoms[0];
                    //printf("  cmd: ");

                    /*for (int i=0; at->argv && at->argv[i]; i++) {
                        printf("%s ", at->argv[i]);
                    }

                    if (at->infile) {
                        printf("< %s ", at->infile);
                    }

                    if (at->outfile) {
                        printf("%s %s ", at->append?">>":">", at->outfile);
                    }*/

                if (!at->infile && !at->outfile && at->argv && at->argv[0] && strcmp(at->argv[0], "hop") == 0) {
                    handle_hop(dir_name, at->argv, shell_home);  
                        //printf("NEW DIR_NAME: %s\n", dir_name);            
                }
                else if (!at->infile && !at->outfile && at->argv && at->argv[0] && strcmp(at->argv[0], "reveal") == 0) {
                    //printf("\n");
                    int a = 0, l=0;
                    char* pathname = (char*) malloc (sizeof(char)*1024);

                    assign_pathname(pathname, at, &a, &l, dir_name, shell_home, prev_dir_reveal);
                        
                    handle_reveal(pathname, a, l, prev_dir_reveal);              
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

                    if (kill(pid, sig) == -1) {
                        printf("Invalid syntax!");
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
                else if (!at->infile && !at->outfile && at->argv && at->argv[0]) {
                    //printf("I'M HERE!!\n");
                    int rc = fork();
                    *fg_pid = rc;

                    char fg_cmdline[1024];
                    snprintf(fg_cmdline, sizeof(fg_cmdline), "%s", input);

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
                        waitpid(rc, &status, WUNTRACED);  // wait for exit or stop

                        signal(SIGINT, SIG_IGN);
                        signal(SIGTSTP, SIG_IGN);
                        *fg_pid = -1;                        
                    }
                }
                else
                {
                    //printf("CALLING SINGLE INPUT OUTPUT REDIRECTION\n");
                    execute_command(tokens, at, count, start, logs, dir_name, cwd, shell_home, &parse_error, jobs, job_count, next_job_id, fg_pid, true);
                }
            }
            else
            {
                if (cmd.types[0] == T_SEMI)
                {
                        //printf("SEQUENTIAL\n");
                    execute_sequential(&cmd, tokens, g, count, start, logs, cwd, dir_name, shell_home, &parse_error, jobs, job_count, next_job_id, fg_pid, true);
                }
                else
                {
                        //printf("PIPELINING\n");
                    execute_pipeline(tokens, &cmd, g, count, start, logs, dir_name, cwd, shell_home, &parse_error, jobs, job_count, next_job_id, fg_pid, true);
                }
            }

                //printf("\n");
        }
    } else {
        if (!parse_error) printf("Invalid Syntax!\n");
    }
        
}
