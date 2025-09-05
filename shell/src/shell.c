#include "shell.h"

#define MAXTOK 1024

Token tokens[MAXTOK];

int main()
{
    char user[1024];
    char systemName[1024];
    char cwd[1024];

    Job jobs[64];
    int job_count = 0;
    int next_job_id = 1;

    ////////////// LLM Generated Code Begins ///////////////

    strcpy(user, getenv("USER"));
    
    if (gethostname(systemName, 1024) != 0)
    {
        printf("Environment Variable (HOSTNAME) error.");
        exit(1);
    }

    //printf("USER: %s\n", user);
    //printf("SYSTEMNAME: %s\n", systemName);

    ////////////// LLM Generated Code Ends ///////////////

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

    test_state(&start, &count, logs);

    while (1)
    {   
        check_jobs(jobs, &job_count);
        
        char* display = make_init_display(user, systemName, dir_name);

        printf("%s", display);
        fflush(stdout);
        free(display);

        char* input = malloc(1024);
        scanf("%1023[^\n]", input);

        if (strcmp(input, "STOP") == 0)
        {
            break;
        }

        scanf("%*[^\n]");
        scanf("%*c");

        int pos = 0;
        int parse_error = 0;
        int ntok = 0;

        tokenize(input, tokens, &ntok);
        char cmds[4097] = "";

        inspect_tokens(tokens, &ntok, cmds, 4097);

        add_log(input, logs, &start, &count); 

        test_state(&start, &count, logs);

        ShellCmd cmd = parse_shell_cmd(tokens, &pos, &parse_error);

        if (!parse_error && peek(tokens,&pos)->type == T_EOF) {
            printf("Parsed %d group(s)\n", cmd.ngroups);

            for (int g=0; g<cmd.ngroups; g++) {

                if (cmd.groups[g].trailing_amp)
                {
                    do_in_bg(cmd.groups[g], jobs, &job_count, &next_job_id, dir_name, cwd, shell_home);
                    continue;
                }

                if (cmd.groups[g].natoms == 1) {
                    Atomic *at = &cmd.groups[g].atoms[0];
                    printf("  cmd: ");

                    for (int i=0; at->argv && at->argv[i]; i++) {
                        printf("%s ", at->argv[i]);
                    }

                    if (at->infile) {
                        printf("< %s ", at->infile);
                    }

                    if (at->outfile) {
                        printf("%s %s ", at->append?">>":">", at->outfile);
                    }

                    if (!at->infile && !at->outfile && at->argv && at->argv[0] && strcmp(at->argv[0], "hop") == 0) {
                        handle_hop(dir_name, at->argv, shell_home);  
                        printf("NEW DIR_NAME: %s\n", dir_name);            
                    }
                    else if (!at->infile && !at->outfile && at->argv && at->argv[0] && strcmp(at->argv[0], "reveal") == 0) {
                        printf("\n");
                        int a = 0, l=0;
                        char* pathname = (char*) malloc (sizeof(char)*1024);

                        assign_pathname(pathname, at, &a, &l, dir_name, shell_home);
                        
                        handle_reveal(pathname, a, l);              
                    }
                    else if (!at->infile && !at->outfile && at->argv && at->argv[0] && strcmp(at->argv[0], "log") == 0) {
                        printf("\n");

                        printf("START: %d, COUNT: %d\n", start, count);
                        int num = get_num(at->argv[2]);
                        printf("NUM: %d\n", num);

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

                            if (num == -1)
                            {
                                syntax_error("Invalid syntax", &parse_error);
                            }
                            else
                            {
                                int idx = ((count - start)%15 +(num - 1)%15)%15;
                                printf("IDX: %d\n", idx);

                                execute_fn(logs[idx], tokens, jobs, &job_count, &next_job_id, dir_name, cwd, shell_home);
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
                    else if (!at->infile && !at->outfile && at->argv && at->argv[0]) {
                        int rc = fork();

                        if (rc == 0) {
                        // child
                            setpgid(0, 0);  // new process group
                            execvp(at->argv[0], at->argv);
                            perror("execvp");
                            exit(1);
                        } else {
                            // parent
                            fg_pid = rc;   // mark foreground job
                            waitpid(rc, NULL, WUNTRACED); // wait (stop/exit)
                            fg_pid = -1;   // reset after it’s done
                        }
                    }
                    else
                    {
                        printf("CALLING SINGLE INPUT OUTPUT REDIRECTION\n");
                        execute_command(tokens, at, &count, &start, logs, dir_name, cwd, shell_home, &parse_error, jobs, &job_count, &next_job_id);
                    }
                }
                else
                {
                    if (cmd.types[0] == T_SEMI)
                    {
                        printf("SEQUENTIAL\n");
                        execute_sequential(&cmd, tokens, g, &count, &start, logs, cwd, dir_name, shell_home, &parse_error, jobs, &job_count, &next_job_id);
                    }
                    else
                    {
                        printf("PIPELINING\n");
                        execute_pipeline(tokens, &cmd, g, &count, &start, logs, dir_name, cwd, shell_home, &parse_error, jobs, &job_count, &next_job_id);
                    }
                }

                printf("\n");
            }
        } else {
            if (!parse_error) printf("Invalid Syntax!\n");
        }

        free(input);
    }

    update_logs(&start, &count, logs, shell_home);

    free(dir_name);

    return 0;
}