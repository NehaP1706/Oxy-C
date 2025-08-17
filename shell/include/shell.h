#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

////////////// LLM Generated Code Begins ///////////////

typedef enum {
    T_EOF = 0,
    T_NAME,
    T_PIPE,      
    T_AMP,       
    T_AND_AND,   
    T_LT,        
    T_GT,        
    T_GT_GT,
    T_SEMI     
} TokenType;

typedef struct {
    TokenType type;
    char* text;  
} Token;

typedef struct {
    char **argv;       
    char *infile;      
    char *outfile;     
    int append;        
} Atomic;

typedef struct {
    Atomic *atoms;    
    int natoms;
} CmdGroup;

typedef struct {
    CmdGroup *groups;  
    int ngroups;
    int trailing_amp;
} ShellCmd;

void add_token(TokenType type, const char* start, int len, Token* tokens, int* ntok);
void tokenize(const char *s, Token* tokens, int* ntok);
CmdGroup parse_cmd_group(Token* tokens, int* pos, int* parse_error);
ShellCmd parse_shell_cmd(Token* tokens, int* pos, int* parse_error);
Atomic parse_atomic(Token* tokens, int* pos, int* parse_error);
Token* peek(Token* tokens, int* pos);
Token* get(Token* tokens, int* pos);
void syntax_error(const char *msg, int* parse_error);

////////////// LLM Generated Code Ends ///////////////

char* make_init_dir_name();
char* make_init_display(char* user, char* systemName, char* dir_name);