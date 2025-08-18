#include "shell.h"

char* make_init_dir_name()
{
    char* dir_name = (char*) malloc (sizeof(char)*1000);

    char cwd[1024];
    getcwd(cwd, sizeof(cwd));

    int n = strlen(cwd);

    if (n>5 && cwd[0] == '/' && cwd[1] == 'h' && cwd[2] == 'o' && cwd[3] == 'm' && cwd[4] == 'e' && cwd[5] == '/')
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
            if (*(p+1) == '&') { add_token(T_AND_AND, NULL, 0, tokens, ntok); p+=2; }
            else { add_token(T_AMP, NULL, 0, tokens, ntok); p++; }
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
            const char *start = p;
            if (*p == '"')
            {
                p++;
                do {
                    p++;
                }
                while (*p && !strchr("|&><;", *p) && *p != '"');
                p++;
                add_token(T_NAME, start, p-start, tokens, ntok);
            }
            else if (*p == '\'')
            {
                p++;
                do{
                    p++;
                }while (*p && !strchr("|&><;", *p) && *p != '\'');
                p++;
                add_token(T_NAME, start, p-start, tokens, ntok);
            }
            else
            {
                while (*p && !isspace(*p) && !strchr("|&><;", *p)) p++;
                add_token(T_NAME, start, p-start, tokens, ntok);
            }
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

    while (!(*parse_error) && (peek(tokens, pos)->type == T_AMP || peek(tokens, pos)->type == T_AND_AND)) {
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

    for (i = 1; argv[i] != NULL; i++) {
        char *target = argv[i];

        if (strcmp(target, "-") != 0) {
            strcpy(last_dir, cwd);
        }

        printf("COMMAND: %s %s\n", argv[0], target);

        if (strcmp(target, "~") == 0 || strcmp(target, shell_home) == 0) {
            if (chdir(shell_home) == 0) {
                strcpy(cwd, "~");
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

////////////// LLM Generated Code Ends ///////////////