#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>     
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/select.h>
#include <fcntl.h>

////////////// LLM Generated Code Begins ///////////////

typedef enum {
    T_EOF = 0,
    T_NAME, //1
    T_PIPE,  //2    
    T_AMP,      //3   
    T_LT,        //4
    T_GT,        //5
    T_GT_GT, //6
    T_SEMI,     //7
    T_AND_AND
} TokenType;

typedef enum { RUNNING, STOPPED } JobState;

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
    int trailing_amp;
} CmdGroup;

typedef struct {
    CmdGroup *groups;  
    int ngroups;
    TokenType* types;
} ShellCmd;

typedef struct {
    int job_id;
    pid_t pid;
    char cmdline[1024]; 
    JobState state;
} Job;

void add_token(TokenType type, const char* start, int len, Token* tokens, int* ntok);
void tokenize(const char *s, Token* tokens, int* ntok);
CmdGroup parse_cmd_group(Token* tokens, int* pos, int* parse_error);
ShellCmd parse_shell_cmd(Token* tokens, int* pos, int* parse_error);
Atomic parse_atomic(Token* tokens, int* pos, int* parse_error);
Token* peek(Token* tokens, int* pos);
Token* get(Token* tokens, int* pos);
void syntax_error(const char *msg, int* parse_error);
void handle_hop(char* cwd, char **argv, char* shell_home);
void handle_reveal(char* pathname, int a, int l);
void add_log(char* cmd, char logs[15][4097], int* start, int* count); 
void test_state(int* start, int* count, char logs[15][4097]);
void update_logs(int* start, int* count, char logs[15][4097], char* shell_home);
void inspect_tokens(Token tokens[1024], int* ntok, char cmds[4097], int size);
void assign_pathname(char* pathname, Atomic* at, int* a, int* l, char* dir_name, char* shell_home);
int get_num(char* input);
void execute_fn(char* input, Token* tokens, Job* jobs, int* job_count, int* next_job_id, char* dir_name, char* cwd, char* shell_home);
void execute_command(Token* tokens, Atomic* at, int* count, int* start, char logs[15][4097], char* dir_name, char*cwd, char* shell_home, int* parse_error, Job* jobs, int* job_count, int* next_job_id);
void execute_command_no_log(Atomic* at, char* dir_name, char* shell_home, int* parse_error, Job* jobs, int* job_count, int* next_job_id);
void execute_pipeline(Token* tokens, ShellCmd *cmd, int g, int* count, int* start, char logs[15][4097], char* dir_name, char* cwd, char* shell_home, int* parse_error, Job* jobs, int* job_count, int* next_job_id);
void execute_pipeline_no_log(ShellCmd *cmd, int g, char* dir_name, char* cwd, char* shell_home, int* parse_error, Job* jobs, int* job_count, int* next_job_id);
void execute_sequential(ShellCmd *cmd, Token* tokens, int g, int* count, int* start, char logs[15][4097], char* cwd, char* dir_name, char* shell_home, int* parse_error, Job* jobs, int* job_count, int* next_job_id);
void execute_sequential_no_log(ShellCmd *cmd, int g, char* cwd, char* dir_name, char* shell_home, int* parse_error, Job* jobs, int* job_count, int* next_job_id);
void do_in_bg(CmdGroup g, Job *jobs, int *job_count, int *next_job_id, char* dir_name, char* cwd, char* shell_home);
void check_jobs(Job *jobs, int *job_count);

////////////// LLM Generated Code Ends ///////////////

char* make_init_dir_name(char* shell_home);
char* make_init_display(char* user, char* systemName, char* dir_name);