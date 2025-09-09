// corrected_parser.c
#include "shell.h"

/* --- helper token accessors --- */
Token *peek(Token* tokens, int* pos) {
    return &tokens[*pos];
}

Token *get(Token* tokens, int* pos) {
    return &tokens[(*pos)++];
}

void syntax_error(const char *msg, int* parse_error) {
    (void)msg; // message unused in your earlier code; keep signature
    fprintf(stderr, "Invalid Syntax!\n");
    (*parse_error) = 1;
}

/* --- atomic: name (name | redirection)* --- */
Atomic parse_atomic(Token* tokens, int* pos, int* parse_error) {
    Atomic a = {0};
    a.argv = malloc(sizeof(char*) * 32); /* keep as before */
    int argc = 0;

    if (peek(tokens, pos)->type != T_NAME) {
        syntax_error("expected command name", parse_error);
        return a;
    }

    a.argv[argc++] = strdup(get(tokens, pos)->text);

    while (!(*parse_error)) {
        TokenType t = peek(tokens, pos)->type;

        if (t == T_NAME) {
            a.argv[argc++] = strdup(get(tokens, pos)->text);
        } else if (t == T_LT) {
            get(tokens, pos);
            if (peek(tokens, pos)->type != T_NAME) { syntax_error("expected infile", parse_error); return a; }
            a.infile = strdup(get(tokens, pos)->text);
        } else if (t == T_GT || t == T_GT_GT) {
            int append = (t == T_GT_GT);
            get(tokens, pos);
            if (peek(tokens, pos)->type != T_NAME) { syntax_error("expected outfile", parse_error); return a; }
            a.outfile = strdup(get(tokens, pos)->text);
            a.append = append;
        } else {
            break;
        }
    }

    a.argv[argc] = NULL;
    return a;
}

/* --- cmd_group: atomic ( '|' atomic )* --- */
CmdGroup parse_cmd_group(Token* tokens, int* pos, int* parse_error) {
    CmdGroup g = {0};
    g.atoms = malloc(sizeof(Atomic) * 32);
    g.natoms = 0;

    g.atoms[g.natoms++] = parse_atomic(tokens, pos, parse_error);

    while (!(*parse_error) && peek(tokens, pos)->type == T_PIPE) {
        get(tokens, pos); /* consume '|' */
        if (peek(tokens, pos)->type != T_NAME) {
            syntax_error("expected command after |", parse_error);
            break;
        }
        g.atoms[g.natoms++] = parse_atomic(tokens, pos, parse_error);
    }

    return g;
}

ShellCmd parse_shell_cmd(Token* tokens, int* pos, int* parse_error) {
    ShellCmd sc = {0};
    sc.groups = malloc(sizeof(CmdGroup) * 32);
    sc.types  = malloc(sizeof(TokenType) * 32);
    sc.ngroups = 0;

    sc.trailing_amp = 0;
    sc.bg_until = -1;   /* default: no backgrounded prefix */

    while (!(*parse_error) && peek(tokens, pos)->type != T_EOF) {
        sc.groups[sc.ngroups] = parse_cmd_group(tokens, pos, parse_error);

        TokenType t = peek(tokens, pos)->type;

        if (t == T_SEMI) {
            get(tokens, pos); /* consume ; */
            sc.types[sc.ngroups] = T_SEMI;
            sc.ngroups++;
            continue;
        }
        else if (t == T_AND_AND) {
            get(tokens, pos);
            sc.types[sc.ngroups] = T_AND_AND;
            sc.ngroups++;
            continue;
        }
        else if (t == T_AMP) {
            get(tokens, pos); /* consume & */
            sc.types[sc.ngroups] = T_AMP;

            /* background all groups parsed up to here */
            if (sc.bg_until == -1)
                sc.bg_until = sc.ngroups + 1;

            sc.trailing_amp = 1;
            sc.ngroups++;

            /* ⚠️ look ahead: if next token is ; or &&, keep parsing foreground */
            t = peek(tokens, pos)->type;
            if (t == T_SEMI || t == T_AND_AND) {
                /* don’t skip, let the next loop consume it */
                continue;
            }

            /* if & is immediately before EOF, stop */
            if (t == T_EOF)
                break;

            /* else, next should be a command group */
            continue;
        }
        else {
            sc.types[sc.ngroups] = T_EOF;
            sc.ngroups++;
            break;
        }
    }

    /* Ensure last group marked with EOF if not set */
    if (sc.ngroups > 0 && sc.types[sc.ngroups-1] == 0)
        sc.types[sc.ngroups-1] = T_EOF;

    return sc;
}
