#include "shell.h"

#define MAXTOK 1024

Token tokens[MAXTOK];

int main()
{
    char user[1024];
    char systemName[1024];
    char cwd[1024];

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
                for (int a=0; a<cmd.groups[g].natoms; a++) {
                    Atomic *at = &cmd.groups[g].atoms[a];
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

                    if (at->argv && at->argv[0] && strcmp(at->argv[0], "hop") == 0) {
                        handle_hop(dir_name, at->argv, shell_home);  
                        printf("NEW DIR_NAME: %s\n", dir_name);            
                    }
                    else if (at->argv && at->argv[0] && strcmp(at->argv[0], "reveal") == 0) {
                        printf("\n");
                        int a = 0, l=0;
                        char* pathname = (char*) malloc (sizeof(char)*1024);

                        assign_pathname(pathname, at, &a, &l, dir_name, shell_home);
                        
                        handle_reveal(pathname, a, l);              
                    }
                    else if (at->argv && at->argv[0] && strcmp(at->argv[0], "log") == 0) {
                        printf("\n");

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
                                int idx = (count - start + num + 1)%15;

                                execute_fn(logs[idx]);
                            }
                        }
                    }
                    else if (at->argv && at->argv[0] && strcmp(at->argv[0], "cat") == 0)
                    {
                        printf("\n");
                        handle_cat(at, &parse_error);
                    }
                    else if (at->argv && at->argv[0] && strcmp(at->argv[0], "sleep") == 0)
                    {
                        handle_sleep(at, &parse_error);
                    }

                    printf("\n");
                }
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