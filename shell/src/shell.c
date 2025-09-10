#include "shell.h"

#define MAXTOK 1024

Token tokens[MAXTOK];

pid_t shell_pgid;

int fg_pid = -1;
Job jobs[64];
int job_count = 0;
int next_job_id = 1;
char fg_cmdline[1024];
pid_t shell_pgid;

char prev_dir_reveal[1024] = "";

int main()
{
    //install_handlers();
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    //int pd = getpid();
    //printf("PD: %d\n", pd);

    bool log_enabled = true;

    char user[1024];
    char systemName[1024];
    char cwd[1024];

    strcpy(user, getenv("USER"));
    
    if (gethostname(systemName, 1024) != 0)
    {
        printf("Environment Variable (HOSTNAME) error.");
        exit(1);
    }

    //printf("USER: %s\n", user);
    //printf("SYSTEMNAME: %s\n", systemName);

    char* shell_home = getcwd(cwd, 1024);
    char* dir_name = make_init_dir_name(shell_home);

    FILE* fptr;
    fptr = fopen("logs.txt", "r");

    if (!fptr) {
        fptr = fopen("logs.txt", "w+"); // create empty file
    }

    char logs[15][4097];

    int count = 0;
    int start = 0;

    while (count < 15 && fgets(logs[count], sizeof(logs[count]), fptr)) {
        logs[count][strcspn(logs[count], "\n")] = '\0';
        count++;
    }

    fclose(fptr);

    //test_state(&start, &count, logs);

    shell_pgid = getpid();
    //printf("SHELL PGID: %d\n", shell_pgid);

    while (1)
    {   
        check_jobs(jobs, &job_count);
        
        char* display = make_init_display(user, systemName, dir_name);

        printf("%s", display);
        fflush(stdout);
        free(display);

        char input[1024];
        if (fgets(input, sizeof(input), stdin) == NULL) {
            update_logs(&start, &count, logs, shell_home);
            free(dir_name);
            handle_eof();
        }
        input[strcspn(input, "\n")] = '\0';

        //scanf("%*[^\n]");
        //scanf("%*c");

        int pos = 0;
        int parse_error = 0;
        int ntok = 0;

        tokenize(input, tokens, &ntok);
        //char cmds[4097] = "";

        //inspect_tokens(tokens, &ntok, cmds, 4097);

        add_log(input, logs, &start, &count); 

        //test_state(&start, &count, logs);

        ShellCmd cmd = parse_shell_cmd(tokens, &pos, &parse_error);

        if (!parse_error && peek(tokens,&pos)->type == T_EOF) {
            //printf("Parsed %d group(s)\n", cmd.ngroups);

            if (cmd.trailing_amp == 1 && cmd.bg_until != -1) {
                //printf("Happening");
                do_in_bg(cmd.bg_until, &cmd, jobs, &job_count, &next_job_id, dir_name, cwd, shell_home, tokens, &count, &start, logs, &parse_error, log_enabled, &fg_pid);
            }
            else
            {
                cmd.bg_until = 0;
            }

            for (int g=cmd.bg_until; g<cmd.ngroups; g++) {

                if (cmd.groups[g].natoms == 1) {
                    Atomic *at = &cmd.groups[g].atoms[0];
                    /*printf("  cmd: ");

                    for (int i=0; at->argv && at->argv[i]; i++) {
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
                        
                        if (strcmp(dir_name, shell_home) == 0)
                        {
                            strcpy(dir_name, "~");
                        }
                    }
                    else if (!at->infile && !at->outfile && at->argv && at->argv[0] && strcmp(at->argv[0], "reveal") == 0) {
                        //printf("\n");
                        int a = 0, l=0;
                        char* pathname = (char*) malloc (sizeof(char)*1024);

                        assign_pathname(pathname, at, &a, &l, dir_name, shell_home, prev_dir_reveal);
                        
                        handle_reveal(pathname, a, l, prev_dir_reveal);              
                    }
                    else if (!at->infile && !at->outfile && at->argv && at->argv[0] && strcmp(at->argv[0], "log") == 0) {
                        //printf("\n");

                        //printf("START: %d, COUNT: %d\n", start, count);
                        //int num = get_num(at->argv[2]);
                        //printf("NUM: %d\n", num);

                        if (at->argv[1] == NULL)
                        {
                            for (int i=0; i<count; i++)
                            {
                                int idx = (start + i)%15;
                                printf("%s\n", logs[idx]);
                            }
                        }
                        else if (strcmp(at->argv[1], "purge") == 0)
                        {
                            start = 0;
                            count = 0;
                        } 
                        else if (strcmp(at->argv[1], "execute") == 0 && at->argv[2])
                        {
                            int num = get_num(at->argv[2]);

                            if (num == -1 || num > 15)
                            {
                                syntax_error("Invalid syntax", &parse_error);
                            }
                            else
                            {
                                int idx = (count - start) - ((num - 1)%15)%15 - 1;
                                //printf("IDX: %d, COMMAND: %s\n", idx, logs[idx]);

                                execute_fn(idx, tokens, &count, &start, logs, jobs, &job_count, &next_job_id, dir_name, cwd, shell_home, &fg_pid);
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
                            printf("Invalid syntax!\n");
                        }
                    }
                    else if (at->argv[0] && strcmp(at->argv[0], "activities") == 0) {
                        for (int i = 0; i < job_count - 1; i++) {
                            for (int j = i + 1; j < job_count; j++) {
                                if (strcmp(jobs[i].cmdline, jobs[j].cmdline) > 0) {
                                    Job tmp = jobs[i];
                                    jobs[i] = jobs[j];
                                    jobs[j] = tmp;
                                }
                            }
                        }

                        for (int i = 0; i < job_count; i++) {
                            printf("[%d] : %s - %s\n", 
                            jobs[i].pid,
                            jobs[i].cmdline,
                            jobs[i].state == RUNNING ? "Running" : "Stopped");
                        }
                    }
                    else if (strcmp(at->argv[0], "fg") == 0) {
                        int jid = at->argv[1] ? atoi(at->argv[1]) : -1;
                        do_fg(jid, jobs, &job_count, &fg_pid);
                    }
                    else if (strcmp(at->argv[0], "bg") == 0) {
                        int jid = at->argv[1] ? atoi(at->argv[1]) : -1;
                        do_bg(jid, jobs, &job_count);
                    }
                    else if (!at->infile && !at->outfile && at->argv && at->argv[0]) {
                        //printf("I'M HERE!!\n");
                        int rc = fork();
                        fg_pid = rc;

                        char fg_cmdline[1024];
                        snprintf(fg_cmdline, sizeof(fg_cmdline), "%s", input);

                        if (rc == 0) {
                        // child
                            fg_pid = getpid();
                            setpgid(getpid(), getpid());
                            
                            signal(SIGINT, sigint_handler);
                            signal(SIGTSTP, sigtstp_handler);

                            execvp(at->argv[0], at->argv);
                            printf("Command not found!\n");

                            exit(1);
                        } else {
                            fg_pid = rc;
                            setpgid(rc, rc);

                            signal(SIGINT, sigint_handler);
                            signal(SIGTSTP, sigtstp_handler);

                            int status;
                            waitpid(rc, &status, WUNTRACED);  // wait for exit or stop

                            signal(SIGINT, SIG_IGN);
                            signal(SIGTSTP, SIG_IGN);
                            fg_pid = -1;                        
                        }
                    }
                    else
                    {
                        //printf("CALLING SINGLE INPUT OUTPUT REDIRECTION\n");
                        execute_command(tokens, at, &count, &start, logs, dir_name, cwd, shell_home, &parse_error, jobs, &job_count, &next_job_id, &fg_pid, log_enabled);
                    }
                }
                else
                {
                    if (cmd.types[0] == T_SEMI)
                    {
                        //printf("SEQUENTIAL\n");
                        execute_sequential(&cmd, tokens, g, &count, &start, logs, cwd, dir_name, shell_home, &parse_error, jobs, &job_count, &next_job_id, &fg_pid, log_enabled);
                    }
                    else
                    {
                        //printf("PIPELINING\n");
                        execute_pipeline(tokens, &cmd, g, &count, &start, logs, dir_name, cwd, shell_home, &parse_error, jobs, &job_count, &next_job_id, &fg_pid, log_enabled);
                    }
                }

                //printf("\n");
            }
        } else {
            if (!parse_error) printf("Invalid Syntax!\n");
        }

        //free(input);
        update_logs(&start, &count, logs, shell_home);
    }

    return 0;
}