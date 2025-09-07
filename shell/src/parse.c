#include "shell.h"

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

    if (!(*parse_error)) {
        Token *t = peek(tokens, pos);
        if (t->type == T_AMP) {
            g.trailing_amp = 1;
            get(tokens, pos); 
        } else if (t->type == T_AND_AND) { 
            g.trailing_amp = 2;
            get(tokens, pos); 
        }
    }

    return g;
}

ShellCmd parse_shell_cmd(Token* tokens, int* pos, int* parse_error) {
    ShellCmd sc = {0};
    sc.groups = malloc(sizeof(CmdGroup) * 32);
    sc.types  = malloc(sizeof(TokenType) * 32);
    sc.ngroups = 0;

    sc.groups[sc.ngroups++] = parse_cmd_group(tokens, pos, parse_error);

    while (!(*parse_error)) {
        TokenType t = peek(tokens, pos)->type;

        if (t == T_SEMI) {
            get(tokens, pos); 
            sc.types[sc.ngroups-1] = T_SEMI;

            if (peek(tokens, pos)->type == T_NAME) {
                sc.groups[sc.ngroups++] = parse_cmd_group(tokens, pos, parse_error);
            } else {
                syntax_error("expected command after ;", parse_error);
                break;
            }
        }
        else if (t == T_AMP || t == T_AND_AND) {
            TokenType sep = get(tokens, pos)->type;
            sc.types[sc.ngroups-1] = sep;

            if (sep == T_AMP)
                sc.groups[sc.ngroups-1].trailing_amp = 1;

            if (peek(tokens, pos)->type == T_NAME) {
                sc.groups[sc.ngroups++] = parse_cmd_group(tokens, pos, parse_error);
            } else {
                if (sep == T_AND_AND) {
                    syntax_error("expected command after &&", parse_error);
                }
                break;
            }
        }
        else {
            break;
        }
    }

    sc.types[sc.ngroups-1] = T_EOF;
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
