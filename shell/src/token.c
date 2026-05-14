#include "shell.h"

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
        } 
        else if (*p == '"' || *p == '\'') {
            const char qstart = *p;
            p++;

            const char* start = p;
            while (*p && *p != qstart) {
                p++;
            }

            add_token(T_NAME, start, p - start, tokens, ntok);

            if (*p == qstart) {
                p++;
            }
        }else {
            const char* start = p;
            while (*p && !isspace(*p) && !strchr("|&><;", *p)) p++;
            add_token(T_NAME, start, p-start, tokens, ntok);
        }
    }
    add_token(T_EOF, NULL, 0, tokens, ntok);
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