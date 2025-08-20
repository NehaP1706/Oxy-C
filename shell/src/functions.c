#include "shell.h"

char* make_init_dir_name(char* shell_home)
{
    char* dir_name = (char*) malloc (sizeof(char)*1000);

    char cwd[1024];
    getcwd(cwd, sizeof(cwd));

    //int n = strlen(cwd);

    if (strcmp(shell_home, cwd) == 0)
    {
        strcpy(dir_name, "~");   
    }
    else
    {
        strcpy(dir_name, cwd);
    }

    return dir_name;
}

char* make_init_display(char* user, char* systemName, char* dir_name)
{
    char* display = (char*) malloc (sizeof(char)*1002048);

    strcpy(display, "<");
    strcat(display, user);
    strcat(display, "@");
    strcat(display, systemName);
    strcat(display, ":");
    strcat(display, dir_name);
    strcat(display, "> ");

    return display;
}

////////////// LLM Generated Code Begins ///////////////

void add_token(TokenType type, const char *start, int len, Token* tokens, int* ntok) {
    tokens[*ntok].type = type;
    if (type == T_NAME) {
        tokens[*ntok].text = strndup(start, len);
    } else {
        tokens[*ntok].text = NULL;
    }
    (*ntok)++;
}

void tokenize(const char *s, Token* tokens, int* ntok) {
    const char *p = s;
    while (*p) {
        if (isspace(*p)) { p++; continue; }
        if (*p == '&') {
            //if (*(p+1) == '&') { add_token(T_AND_AND, NULL, 0, tokens, ntok); p+=2; }
            //else 
            { add_token(T_AMP, NULL, 0, tokens, ntok); p++; }
        } else if (*p == '|') {
            add_token(T_PIPE, NULL, 0, tokens, ntok); p++;
        } else if (*p == '<') {
            add_token(T_LT, NULL, 0, tokens, ntok); p++;
        } else if (*p == '>') {
            if (*(p+1) == '>') { add_token(T_GT_GT, NULL, 0, tokens, ntok); p+=2; }
            else { add_token(T_GT, NULL, 0, tokens, ntok); p++; }
        }
        else if (*p == ';') {
            add_token(T_SEMI, NULL, 0, tokens, ntok);
            p++;
        } else {
            const char* start = p;
            while (*p && !isspace(*p) && !strchr("|&><;", *p)) p++;
            add_token(T_NAME, start, p-start, tokens, ntok);
        }
    }
    add_token(T_EOF, NULL, 0, tokens, ntok);
}

Atomic parse_atomic(Token* tokens, int* pos, int* parse_error) {
    Atomic a = {0};
    a.argv = malloc(sizeof(char*)*32);
    int argc = 0;

    if (peek(tokens, pos)->type != T_NAME) {
        syntax_error("expected command name", parse_error);
        return a;
    }
    a.argv[argc++] = strdup(get(tokens, pos)->text);

    while (!(*parse_error)) {
        if (peek(tokens, pos)->type == T_NAME) {
            a.argv[argc++] = strdup(get(tokens, pos)->text);
        } else if (peek(tokens, pos)->type == T_LT) {
            get(tokens, pos);
            if (peek(tokens, pos)->type != T_NAME) { syntax_error("expected infile", parse_error); return a; }
            a.infile = strdup(get(tokens, pos)->text);
        } else if (peek(tokens, pos)->type == T_GT || peek(tokens, pos)->type == T_GT_GT) {
            int append = (peek(tokens, pos)->type == T_GT_GT);
            get(tokens, pos);
            if (peek(tokens, pos)->type != T_NAME) { syntax_error("expected outfile", parse_error); return a; }
            a.outfile = strdup(get(tokens, pos)->text);
            a.append = append;
        } else break;
    }

    a.argv[argc] = NULL;
    return a;
}

CmdGroup parse_cmd_group(Token* tokens, int* pos, int* parse_error) {
    CmdGroup g = {0};
    g.atoms = malloc(sizeof(Atomic)*32);
    g.natoms = 0;

    g.atoms[g.natoms++] = parse_atomic(tokens, pos, parse_error);

    while (!(*parse_error) && peek(tokens, pos)->type == T_PIPE) {
        get(tokens, pos); // consume |
        if (peek(tokens,pos)->type != T_NAME) { syntax_error("expected command after |", parse_error); break; }
        g.atoms[g.natoms++] = parse_atomic(tokens, pos, parse_error);
    }
    return g;
}

ShellCmd parse_shell_cmd(Token* tokens, int* pos, int* parse_error) {
    ShellCmd sc = {0};
    sc.groups = malloc(sizeof(CmdGroup)*32);
    sc.ngroups = 0;

    sc.groups[sc.ngroups++] = parse_cmd_group(tokens, pos, parse_error);

    while (!(*parse_error) && peek(tokens, pos)->type == T_SEMI) {
        get(tokens, pos); // consume ';'
        if (peek(tokens,pos)->type == T_NAME) {
            sc.groups[sc.ngroups++] = parse_cmd_group(tokens, pos, parse_error);
        } else {
            syntax_error("expected command after ;", parse_error);
            break;
        }
    }

    while (!(*parse_error) && (peek(tokens, pos)->type == T_AMP)){ //|| peek(tokens, pos)->type == T_AND_AND)) {
        TokenType sep = get(tokens, pos)->type;
        if (peek(tokens, pos)->type == T_NAME) {
            sc.groups[sc.ngroups++] = parse_cmd_group(tokens, pos, parse_error);
        } else {
            if (sep == T_AMP) sc.trailing_amp = 1;
            else syntax_error("expected command after &&", parse_error);
            break;
        }
    }
    return sc;
}


Token *peek(Token* tokens, int* pos) {
    return &tokens[*pos]; 
}

Token *get(Token* tokens, int* pos) {
    return &tokens[(*pos)++];
}

void syntax_error(const char *msg, int* parse_error) {
    fprintf(stderr, "Invalid Syntax!\n");
    (*parse_error) = 1;
}

void handle_hop(char* cwd, char **argv, char* shell_home) {
    static char last_dir[1024] = "";
    char temp[1024];
    int i;

    if (strcmp(cwd, "~") == 0)
    {
        strcpy(cwd, "~");
    }

    for (i = 1; argv[i] != NULL; i++) {
        char *target = argv[i];

        if (strcmp(target, "-") != 0) {
            strcpy(last_dir, cwd);
        }

        printf("COMMAND: %s %s\n", argv[0], target);

        if (strcmp(target, "~") == 0 || strcmp(target, shell_home) == 0) {
            if (chdir(shell_home) == 0) {
                strcpy(cwd, "~");
                //strcpy(cwd, shell_home);
            } else {
                perror("hop");
            }
        }
        else if (strcmp(target, "-") == 0) {
            if (strlen(last_dir) == 0) {
                continue;
            }
            strcpy(temp, cwd);
            if (chdir(last_dir) == 0) {
                getcwd(cwd, 1024);
                strcpy(last_dir, temp);   
                printf("%s\n", cwd);
            } else {
                perror("hop");
            }
        }
        else {
            if (chdir(target) == 0) {
                getcwd(cwd, 1024);
                if (strcmp(target, shell_home) == 0)
                {
                    strcpy(cwd, "~");
                }
            } else {
                perror("hop");
            }
        }
    }
}

int cmpfunc(const void *a, const void *b) {
    return strcmp(*(char **)a, *(char **)b);
}

void handle_reveal(char *path, int a, int l) {  
    printf("FLAGS : a %d l %d\n", a, l);
    
    DIR *dir = opendir(path);

    if (!dir) {
        printf("No such directory!\n");
        return;
    }

    struct dirent *entry;
    char *files[1024];
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (!a && entry->d_name[0] == '.')
            continue;
        files[count++] = strdup(entry->d_name);
    }
    closedir(dir);

    qsort(files, count, sizeof(char *), cmpfunc);

    for (int i = 0; i < count; i++) {
        if (l)
            printf("%s\n", files[i]);
        else
            printf("%s ", files[i]);
        free(files[i]);
    }
    if (!l) printf("\n");
}

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
        printf("Just updated index1: %d\n", (*start + *count)%15);
        (*count)++;
    } else {
        strcpy(logs[*start], cmd);
        printf("Just updated index2: %d\n", (*start));
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

void inspect_tokens(Token tokens[1024], int* ntok, char cmds[4097], int size)
{
    printf("%d", *ntok);
    for (int i=0; i<(*ntok); i++)
    {
        printf("Token Type: %d, Token text: %s\n", tokens[i].type, tokens[i].text);
        if (tokens[i].text != NULL)
        {
            strncat(cmds, tokens[i].text, size-strlen(cmds)-1);
            strncat(cmds, " ", size-strlen(cmds)-1);
        }
        else if (tokens[i].type == 2)
        {
            strncat(cmds, "|", size-strlen(cmds)-1);
            strncat(cmds, " ", size-strlen(cmds)-1);
        }
        else if (tokens[i].type == 3)
        {
            strncat(cmds, "&", size-strlen(cmds)-1);
            strncat(cmds, " ", size-strlen(cmds)-1);
        }
        else if (tokens[i].type == 4)
        {
            strncat(cmds, "<", size-strlen(cmds)-1);
            strncat(cmds, " ", size-strlen(cmds)-1);
        }
        else if (tokens[i].type == 5)
        {
            strncat(cmds, ">", size-strlen(cmds)-1);
            strncat(cmds, " ", size-strlen(cmds)-1);
        }
        else if (tokens[i].type == 6)
        {
            strncat(cmds, ">>", size-strlen(cmds)-1);
            strncat(cmds, " ", size-strlen(cmds)-1);
        }
        else if (tokens[i].type == 7)
        {
            strncat(cmds, ";", size-strlen(cmds)-1);
            strncat(cmds, " ", size-strlen(cmds)-1);
        }
    }

    printf("GATHERED COMMAND: %s\n", cmds);
}

void assign_pathname(char* pathname, Atomic* at, int* a, int* l, char* dir_name, char* shell_home)
{
    strcpy(pathname, (strcmp(dir_name,"~") == 0 ? shell_home : dir_name));

    for (int i=1; at->argv[i] != NULL; i++)
    {
        if (at->argv[i][0] == '-') {
            for (int j = 1; at->argv[i][j] != '\0'; j++) {
                if (at->argv[i][j] == 'a') {
                    *a = 1;
                } else if (at->argv[i][j] == 'l') {
                    *l = 1;
                }
            }
        } else {
            strcpy(pathname, at->argv[i]);
        }   
    }
}

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

void execute_fn(char* cmd)
{
    ;
}

void handle_cat(Atomic* at, int* parse_error)
{
    if (at->argv[1] == NULL)
    {
        syntax_error("Invalid Syntax!", parse_error);
    }
    else
    {
        FILE* fptr = fopen(at->argv[1], "r");

        char str[4097];\
        int reads = 0;

        while((reads = fread(str, 1, 4097, fptr)) > 0)
        {
            fwrite(str, 1, reads, stdout);
        }

        fclose(fptr);
    }
}

void handle_sleep(Atomic* at, int* parse_error)
{
    if (at->argv[1] == NULL)
    {
        syntax_error("Invalid Syntax!", parse_error);
    }
    else
    {
        int n = get_num(at->argv[1]);

        struct timeval tv;
        tv.tv_sec = n; 
        tv.tv_usec = 0;

        select(0, NULL, NULL, NULL, &tv);
    }
}

////////////// LLM Generated Code Ends ///////////////
