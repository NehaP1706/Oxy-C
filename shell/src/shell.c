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

    char* dir_name = make_init_dir_name();
    char* shell_home = getcwd(cwd, 1024);

    while (1)
    {   
        char* display = make_init_display(user, systemName, dir_name);

        printf("%s", display);
        free(display);

        char* input = malloc(1024);
        scanf("%1023[^\n]", input);

        scanf("%*[^\n]");
        scanf("%*c");

        int pos = 0;
        int parse_error = 0;
        int ntok = 0;

        tokenize(input, tokens, &ntok);

        for (int i=0; i<ntok; i++)
        {
            printf("Token Type: %d, Token text: %s\n", tokens[i].type, tokens[i].text);
        }

        ShellCmd cmd = parse_shell_cmd(tokens, &pos, &parse_error);

        if (!parse_error && peek(tokens,&pos)->type == T_EOF) {
            printf("Parsed %d group(s)\n", cmd.ngroups);
            for (int g=0; g<cmd.ngroups; g++) {
                for (int a=0; a<cmd.groups[g].natoms; a++) {
                    Atomic *at = &cmd.groups[g].atoms[a];
                    printf("  cmd: ");
                    for (int i=0; at->argv && at->argv[i]; i++)
                        printf("%s ", at->argv[i]);
                    if (at->infile) printf("< %s ", at->infile);
                    if (at->outfile) printf("%s %s ", at->append?">>":">", at->outfile);

                    if (at->argv && at->argv[0] && strcmp(at->argv[0], "hop") == 0) {
                        handle_hop(dir_name, at->argv, shell_home);  
                        printf("NEW DIR_NAME: %s\n", dir_name);            
                    }

                    if (at->argv && at->argv[0] && strcmp(at->argv[0], "reveal") == 0) {
                        printf("\n");
                        int a = 0, l=0;
                        char* pathname = (char*) malloc (sizeof(char)*1024);
                        strcpy(pathname, (strcmp(dir_name,"~") == 0 ? shell_home : dir_name));

                        for (int i=1; at->argv[i] != NULL; i++)
                        {
                            if (at->argv[i][0] == '-') {
                                for (int j = 1; at->argv[i][j] != '\0'; j++) {
                                    if (at->argv[i][j] == 'a') {
                                        a = 1;
                                    } else if (at->argv[i][j] == 'l') {
                                        l = 1;
                                    }
                            }
                            } else {
                                strcpy(pathname, at->argv[i]);
                            }
                        }
                        
                        handle_reveal(pathname, a, l);  
                        printf("NEW DIR_NAME: %s\n", dir_name);            
                    }

                    printf("\n");
                }
            }
        } else {
            if (!parse_error) printf("Invalid Syntax!\n");
        }

        scanf("%*[^\n]");
        scanf("%*c");

        free(input);
    }

    free(dir_name);

    return 0;
}