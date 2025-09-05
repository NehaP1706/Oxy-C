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

void execute_fn(char* input, Token* tokens, Job* jobs, int* job_count, int* next_job_id, char* dir_name, char* cwd, char* shell_home)
{
    int pos = 0;
    int parse_error = 0;
    int ntok = 0;

    tokenize(input, tokens, &ntok);
    char cmds[4097] = "";

    inspect_tokens(tokens, &ntok, cmds, 4097);

    ShellCmd cmd = parse_shell_cmd(tokens, &pos, &parse_error);

    if (!parse_error && peek(tokens,&pos)->type == T_EOF) {
        printf("Parsed %d group(s)\n", cmd.ngroups);

        for (int g=0; g<cmd.ngroups; g++) {
            if (cmd.groups[g].trailing_amp)
            {
                do_in_bg(cmd.groups[g], jobs, job_count, next_job_id, dir_name, cwd, shell_home);
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
                else if (at->argv[0] && strcmp(at->argv[0], "activities") == 0) {
                    for (int i = 0; i < *job_count - 1; i++) {
                        for (int j = i + 1; j < *job_count; j++) {
                            if (strcmp(jobs[i].cmdline, jobs[j].cmdline) > 0) {
                                Job tmp = jobs[i];
                                jobs[i] = jobs[j];
                                jobs[j] = tmp;
                            }
                        }
                    }

                    for (int i = 0; i < *job_count; i++) {
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
                    execute_command_no_log(at, dir_name, shell_home, &parse_error, jobs, job_count, next_job_id);
                }
            }
            else
            {
                if (cmd.types[0] == T_SEMI)
                {
                    printf("SEQUENTIAL\n");
                    execute_sequential_no_log(&cmd, g, cwd, dir_name, shell_home, &parse_error, jobs, job_count, next_job_id);
                }
                else
                {
                    printf("PIPELINING\n");
                    execute_pipeline_no_log(&cmd, g, dir_name, cwd, shell_home, &parse_error, jobs, job_count, next_job_id);
                }
            }

            printf("\n");
        }
    } else {
        if (!parse_error) printf("Invalid Syntax!\n");
    }
}

void execute_command(Token* tokens, Atomic* at, int* count, int* start, char logs[15][4097], char* dir_name, char*cwd, char* shell_home, int* parse_error, Job* jobs, int* job_count, int* next_job_id) {
    pid_t pid = fork();
    
    if (pid == 0) {
        if (at->infile) {
            int fd = open(at->infile, O_RDONLY);
            if (fd < 0) {
                perror("No such file or directory");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        if (at->outfile) {
            int fd;
            if (at->append)
                fd = open(at->outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
            else
                fd = open(at->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (fd < 0) {
                perror("Cannot open output file");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        if (strcmp(at->argv[0], "reveal") == 0)
        {
            int a = 0, l=0;
            char* pathname = (char*) malloc (sizeof(char)*1024);

            assign_pathname(pathname, at, &a, &l, dir_name, shell_home);
                        
            handle_reveal(pathname, a, l);
            free(pathname);
            exit(0);
        }
        else if (at->argv[0] && strcmp(at->argv[0], "log") == 0) {
            printf("\n");

            if (at->argv[1] == NULL)
            {
                for (int i=0; i<*count; i++)
                {
                    int idx = (*start + i)%15;
                    printf("%s\n", logs[idx]);
                }
            }
            else if (strcmp(at->argv[1], "purge") == 0)
            {
                *start = 0;
                *count = 0;
            } 
            else if (strcmp(at->argv[1], "execute") == 0 && at->argv[2])
            {
                int num = get_num(at->argv[2]);
                printf("NUM: %d\n", num);

                if (num == -1)
                {
                    syntax_error("Invalid syntax", parse_error);
                }
                else
                {
                    int idx = ((*count - *start)%15 + (num - 1)%15)%15;
                    printf("EXECUTING LINE: %d", idx);
                    fflush(stdout);

                    execute_fn(logs[idx], tokens, jobs, job_count, next_job_id, dir_name, cwd, shell_home);
                }
            }
        }
        else if (at->argv[0] && strcmp(at->argv[0], "activities") == 0) {
            for (int i = 0; i < *job_count - 1; i++) {
                for (int j = i + 1; j < *job_count; j++) {
                    if (strcmp(jobs[i].cmdline, jobs[j].cmdline) > 0) {
                        Job tmp = jobs[i];
                        jobs[i] = jobs[j];
                        jobs[j] = tmp;
                    }
                }
            }

            for (int i = 0; i < *job_count; i++) {
                printf("[%d] : %s - %s\n", 
                jobs[i].pid,
                jobs[i].cmdline,
                jobs[i].state == RUNNING ? "Running" : "Stopped");
            }
        }
        else 
        {
            execvp(at->argv[0], at->argv);
            perror("execvp failed");
            exit(1);
        }

    } else {
        waitpid(pid, NULL, 0);
    }
}

void execute_command_no_log(Atomic* at, char* dir_name, char* shell_home, int* parse_error, Job* jobs, int* job_count, int* next_job_id) {
    pid_t pid = fork();
    
    if (pid == 0) {
        if (at->infile) {
            int fd = open(at->infile, O_RDONLY);
            if (fd < 0) {
                perror("No such file or directory");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        if (at->outfile) {
            int fd;
            if (at->append)
                fd = open(at->outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
            else
                fd = open(at->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (fd < 0) {
                perror("Cannot open output file");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        if (strcmp(at->argv[0], "reveal") == 0)
        {
            int a = 0, l=0;
            char* pathname = (char*) malloc (sizeof(char)*1024);

            assign_pathname(pathname, at, &a, &l, dir_name, shell_home);
                        
            handle_reveal(pathname, a, l);
            free(pathname);
            exit(0);
        }
        else if (at->argv[0] && strcmp(at->argv[0], "activities") == 0) {
            for (int i = 0; i < *job_count - 1; i++) {
                for (int j = i + 1; j < *job_count; j++) {
                    if (strcmp(jobs[i].cmdline, jobs[j].cmdline) > 0) {
                        Job tmp = jobs[i];
                        jobs[i] = jobs[j];
                        jobs[j] = tmp;
                    }
                }
            }

            for (int i = 0; i < *job_count; i++) {
                printf("[%d] : %s - %s\n", 
                jobs[i].pid,
                jobs[i].cmdline,
                jobs[i].state == RUNNING ? "Running" : "Stopped");
            }
        }
        else 
        {
            execvp(at->argv[0], at->argv);
            perror("execvp failed");
            exit(1);
        }

    } else {
        waitpid(pid, NULL, 0);
    }
}

void execute_pipeline(Token* tokens, ShellCmd *cmd, int g, int* count, int* start, char logs[15][4097], char* dir_name, char* cwd, char* shell_home, int* parse_error, Job* jobs, int* job_count, int* next_job_id) {
    int num_atoms = cmd->groups[g].natoms;
    int pipes[num_atoms-1][2];

    // Create pipes
    for (int i=0; i<num_atoms-1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe failed");
            exit(1);
        }
    }

    for (int a=0; a<num_atoms; a++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(1);
        }
        if (pid == 0) {
            Atomic *at = &cmd->groups[g].atoms[a];

            // stdin from previous pipe (if not first)
            if (a > 0) {
                dup2(pipes[a-1][0], STDIN_FILENO);
            }

            // stdout to next pipe (if not last)
            if (a < num_atoms-1) {
                dup2(pipes[a][1], STDOUT_FILENO);
            }

            // Apply file redirection
            if (at->infile && a == 0) {
                int fd = open(at->infile, O_RDONLY);
                if (fd < 0) { perror("open infile"); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (at->outfile && a == num_atoms-1) {
                int flags = O_WRONLY | O_CREAT;
                if (at->append) {
                    flags |= O_APPEND;
                } else {
                    flags |= O_TRUNC;
                }
                int fd = open(at->outfile, flags, 0644);
                if (fd < 0) { perror("open outfile"); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            for (int i=0; i<num_atoms-1; i++) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }

            if (strcmp(at->argv[0], "hop") == 0)
            {
                handle_hop(dir_name, at->argv, shell_home); 
            }
            else if (strcmp(at->argv[0], "reveal") == 0)
            {
                int a = 0, l=0;
                char* pathname = (char*) malloc (sizeof(char)*1024);

                assign_pathname(pathname, at, &a, &l, dir_name, shell_home);
                        
                handle_reveal(pathname, a, l);
                free(pathname);
            }
            else if (at->argv[0] && strcmp(at->argv[0], "log") == 0) {
                printf("\n");

                if (at->argv[1] == NULL)
                {
                    for (int i=0; i<*count; i++)
                    {
                        int idx = (*start + i)%15;
                        printf("%s\n", logs[idx]);
                    }
                }
                else if (strcmp(at->argv[1], "purge") == 0)
                {
                    *start = 0;
                    *count = 0;
                } 
                else if (strcmp(at->argv[1], "execute") == 0 && at->argv[2])
                {
                    int num = get_num(at->argv[2]);
                    printf("NUM: %d\n", num);

                    if (num == -1)
                    {
                        syntax_error("Invalid syntax", parse_error);
                    }
                    else
                    {
                        int idx = ((*count - *start)%15 + (num - 1)%15)%15;
                        printf("EXECUTING LINE: %d", idx);
                        fflush(stdout);

                        execute_fn(logs[idx], tokens, jobs, job_count, next_job_id, dir_name, cwd, shell_home);
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
                for (int i = 0; i < *job_count - 1; i++) {
                    for (int j = i + 1; j < *job_count; j++) {
                        if (strcmp(jobs[i].cmdline, jobs[j].cmdline) > 0) {
                            Job tmp = jobs[i];
                            jobs[i] = jobs[j];
                            jobs[j] = tmp;
                        }
                    }
                }

                for (int i = 0; i < *job_count; i++) {
                    printf("[%d] : %s - %s\n", 
                    jobs[i].pid,
                    jobs[i].cmdline,
                    jobs[i].state == RUNNING ? "Running" : "Stopped");
                }
            }
            else if (at->argv && at->argv[0])
            {
                printf("CALLING EXEC 2\n");
                execvp(at->argv[0], at->argv);
                printf("EXEC() ERROR.");
            }
            else 
            {
                printf("CALLING EXEC 1\n");
                execvp(at->argv[0], at->argv);
                perror("execvp failed");
                exit(1);
            }
        }
    }

    for (int i=0; i<num_atoms-1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    for (int a=0; a<num_atoms; a++) {
        wait(NULL);
    }
}

void execute_pipeline_no_log(ShellCmd *cmd, int g, char* dir_name, char* cwd, char* shell_home, int* parse_error, Job* jobs, int* job_count, int* next_job_id) {
    int num_atoms = cmd->groups[g].natoms;
    int pipes[num_atoms-1][2];

    // Create pipes
    for (int i=0; i<num_atoms-1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe failed");
            exit(1);
        }
    }

    for (int a=0; a<num_atoms; a++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            exit(1);
        }
        if (pid == 0) {
            Atomic *at = &cmd->groups[g].atoms[a];

            // stdin from previous pipe (if not first)
            if (a > 0) {
                dup2(pipes[a-1][0], STDIN_FILENO);
            }

            // stdout to next pipe (if not last)
            if (a < num_atoms-1) {
                dup2(pipes[a][1], STDOUT_FILENO);
            }

            // Apply file redirection
            if (at->infile && a == 0) {
                int fd = open(at->infile, O_RDONLY);
                if (fd < 0) { perror("open infile"); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (at->outfile && a == num_atoms-1) {
                int flags = O_WRONLY | O_CREAT;
                if (at->append) {
                    flags |= O_APPEND;
                } else {
                    flags |= O_TRUNC;
                }
                int fd = open(at->outfile, flags, 0644);
                if (fd < 0) { perror("open outfile"); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            for (int i=0; i<num_atoms-1; i++) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }

            if (strcmp(at->argv[0], "hop") == 0)
            {
                handle_hop(dir_name, at->argv, shell_home); 
            }
            else if (strcmp(at->argv[0], "reveal") == 0)
            {
                int a = 0, l=0;
                char* pathname = (char*) malloc (sizeof(char)*1024);

                assign_pathname(pathname, at, &a, &l, dir_name, shell_home);
                        
                handle_reveal(pathname, a, l);
                free(pathname);
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
                for (int i = 0; i <* job_count - 1; i++) {
                    for (int j = i + 1; j < *job_count; j++) {
                        if (strcmp(jobs[i].cmdline, jobs[j].cmdline) > 0) {
                            Job tmp = jobs[i];
                            jobs[i] = jobs[j];
                            jobs[j] = tmp;
                        }
                    }
                }

                for (int i = 0; i < *job_count; i++) {
                    printf("[%d] : %s - %s\n", 
                    jobs[i].pid,
                    jobs[i].cmdline,
                    jobs[i].state == RUNNING ? "Running" : "Stopped");
                }
            }
            else if (at->argv && at->argv[0])
            {
                printf("CALLING EXEC 2\n");
                execvp(at->argv[0], at->argv);
                printf("EXEC() ERROR.");
            }
            else 
            {
                printf("CALLING EXEC 1\n");
                execvp(at->argv[0], at->argv);
                perror("execvp failed");
                exit(1);
            }
        }
    }

    for (int i=0; i<num_atoms-1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    for (int a=0; a<num_atoms; a++) {
        wait(NULL);
    }
}

void execute_sequential(ShellCmd *cmd, Token* tokens, int g, int* count, int* start, char logs[15][4097], char* cwd, char* dir_name, char* shell_home, int* parse_error, Job* jobs, int* job_count, int* next_job_id) {
    for (int i = 0; i < cmd->ngroups; i++) {
        if (cmd->groups[g].natoms == 1) {
            Atomic *at = &cmd->groups[g].atoms[0];

            if (at->argv[0] && strcmp(at->argv[0], "hop") == 0) {
                handle_hop(dir_name, at->argv, shell_home);  
                printf("NEW DIR_NAME: %s\n", dir_name);            
            }
            else if (at->argv[0] && strcmp(at->argv[0], "reveal") == 0) {
                printf("\n");
                int a = 0, l=0;
                char* pathname = (char*) malloc (sizeof(char)*1024);

                assign_pathname(pathname, at, &a, &l, dir_name, shell_home);
                        
                handle_reveal(pathname, a, l);              
            }
            else if (at->argv[0] && strcmp(at->argv[0], "log") == 0) {
                printf("\n");

                if (at->argv[1] == NULL)
                {
                    for (int i=0; i<*count; i++)
                    {
                        int idx = (*start + i)%15;
                        printf("%s\n", logs[idx]);
                    }
                }
                else if (strcmp(at->argv[1], "purge") == 0)
                {
                    *start = 0;
                    *count = 0;
                } 
                else if (strcmp(at->argv[1], "execute") == 0 && at->argv[2])
                {
                    int num = get_num(at->argv[2]);
                    printf("NUM: %d\n", num);

                    if (num == -1)
                    {
                        syntax_error("Invalid syntax", parse_error);
                    }
                    else
                    {
                        int idx = ((*count - *start)%15 + (num - 1)%15)%15;
                        printf("EXECUTING LINE: %d", idx);
                        fflush(stdout);

                        execute_fn(logs[idx], tokens, jobs, job_count, next_job_id, dir_name, cwd, shell_home);
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
                for (int i = 0; i < *job_count - 1; i++) {
                    for (int j = i + 1; j < *job_count; j++) {
                        if (strcmp(jobs[i].cmdline, jobs[j].cmdline) > 0) {
                            Job tmp = jobs[i];
                            jobs[i] = jobs[j];
                            jobs[j] = tmp;
                        }
                    }
                }

                for (int i = 0; i < *job_count; i++) {
                    printf("[%d] : %s - %s\n", 
                    jobs[i].pid,
                    jobs[i].cmdline,
                    jobs[i].state == RUNNING ? "Running" : "Stopped");
                }
            }
            else if (at->argv && at->argv[0])
            {
                int rc = fork();

                if (rc == 0)
                {
                    execvp(at->argv[0], at->argv);
                    printf("EXEC() ERROR.");
                }
                else
                {
                    wait(NULL);
                }
            }
            else
            {
                printf("CALLING SINGLE INPUT OUTPUT REDIRECTION\n");
                execute_command(tokens, at, count, start, logs, dir_name, cwd, shell_home, parse_error, jobs, job_count, next_job_id);
                exit(0);
            }
        }
        else
        {
            execute_pipeline(tokens, cmd, g, count, start, logs, dir_name, cwd, shell_home, parse_error, jobs, job_count, next_job_id);
        }
    }
}

void execute_sequential_no_log(ShellCmd *cmd, int g, char* cwd, char* dir_name, char* shell_home, int* parse_error, Job* jobs, int* job_count, int* next_job_id) {
    for (int i = 0; i < cmd->ngroups; i++) {
        if (cmd->groups[g].natoms == 1) {
            Atomic *at = &cmd->groups[g].atoms[0];

            if (at->argv[0] && strcmp(at->argv[0], "hop") == 0) {
                handle_hop(dir_name, at->argv, shell_home);  
                printf("NEW DIR_NAME: %s\n", dir_name);            
            }
            else if (at->argv[0] && strcmp(at->argv[0], "reveal") == 0) {
                printf("\n");
                int a = 0, l=0;
                char* pathname = (char*) malloc (sizeof(char)*1024);

                assign_pathname(pathname, at, &a, &l, dir_name, shell_home);
                        
                handle_reveal(pathname, a, l);              
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
                for (int i = 0; i < *job_count - 1; i++) {
                    for (int j = i + 1; j < *job_count; j++) {
                        if (strcmp(jobs[i].cmdline, jobs[j].cmdline) > 0) {
                            Job tmp = jobs[i];
                            jobs[i] = jobs[j];
                            jobs[j] = tmp;
                        }
                    }
                }

                for (int i = 0; i < *job_count; i++) {
                    printf("[%d] : %s - %s\n", 
                    jobs[i].pid,
                    jobs[i].cmdline,
                    jobs[i].state == RUNNING ? "Running" : "Stopped");
                }
            }
            else if (at->argv && at->argv[0])
            {
                int rc = fork();

                if (rc == 0)
                {
                    execvp(at->argv[0], at->argv);
                    printf("EXEC() ERROR.");
                }
                else
                {
                    wait(NULL);
                }
            }
            else
            {
                printf("CALLING SINGLE INPUT OUTPUT REDIRECTION\n");
                execute_command_no_log(at, dir_name, shell_home, parse_error, jobs, job_count, next_job_id);
                exit(0);
            }
        }
        else
        {
            execute_pipeline_no_log(cmd, g, dir_name, cwd, shell_home, parse_error, jobs, job_count, next_job_id);
        }
    }
}

void do_in_bg(CmdGroup g, Job *jobs, int *job_count, int *next_job_id, char* dir_name, char* cwd, char* shell_home)
{
    pid_t pid = fork();

    if (pid == 0) {
        setpgid(0, 0);

        // --------- single atomic -----------
        if (g.natoms == 1) {
            Atomic *at = &g.atoms[0];

            if (at->infile) {
                int fd = open(at->infile, O_RDONLY);
                if (fd < 0) { perror("No such file or directory"); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            else
            {
                int devnull = open("/dev/null", O_RDONLY);
                dup2(devnull, STDIN_FILENO);
                close(devnull);
            }

            if (at->outfile) {
                int fd;
                if (at->append)
                    fd = open(at->outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
                else
                    fd = open(at->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror("Cannot open output file"); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            if (at->argv && at->argv[0]) {
                if (strcmp(at->argv[0], "hop") == 0) {
                    handle_hop(dir_name, at->argv, shell_home);
                    exit(0);
                }
                else if (strcmp(at->argv[0], "reveal") == 0) {
                    int a = 0, l = 0;
                    char* pathname = malloc(1024);
                    assign_pathname(pathname, at, &a, &l, dir_name, shell_home);
                    handle_reveal(pathname, a, l);
                    free(pathname);
                    exit(0);
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
                    for (int i = 0; i < *job_count - 1; i++) {
                        for (int j = i + 1; j < *job_count; j++) {
                            if (strcmp(jobs[i].cmdline, jobs[j].cmdline) > 0) {
                                Job tmp = jobs[i];
                                jobs[i] = jobs[j];
                                jobs[j] = tmp;
                            }
                        }
                    }

                    for (int i = 0; i < *job_count; i++) {
                        printf("[%d] : %s - %s\n", 
                        jobs[i].pid,
                        jobs[i].cmdline,
                        jobs[i].state == RUNNING ? "Running" : "Stopped");
                    }
                }
                else {
                    execvp(at->argv[0], at->argv);
                    perror("execvp failed");
                    exit(1);
                }
            }
            exit(0);
        }
    }
    else if (pid > 0) {
        jobs[*job_count].pid = pid;
        jobs[*job_count].job_id = (*next_job_id)++;

        // Build full command line string for logging
        jobs[*job_count].cmdline[0] = '\0';
        for (int i = 0; g.atoms[0].argv && g.atoms[0].argv[i]; i++) {
            strcat(jobs[*job_count].cmdline, g.atoms[0].argv[i]);
            strcat(jobs[*job_count].cmdline, " ");
        }

        (*job_count)++;

        printf("[%d] %d\n", jobs[*job_count - 1].job_id, pid);
        fflush(stdout);
    }
    else {
        perror("fork failed");
    }
}

void check_jobs(Job *jobs, int *job_count) {
    int status;
    pid_t result;

    // Reap all finished children
    while ((result = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < *job_count; i++) {
            if (jobs[i].pid == result) {
                if (WIFEXITED(status)) {
                    printf("%s with pid %d exited normally\n",
                           jobs[i].cmdline, jobs[i].pid);
                } else if (WIFSIGNALED(status)) {
                    printf("%s with pid %d exited abnormally\n",
                           jobs[i].cmdline, jobs[i].pid);
                }
                fflush(stdout);

                // Shift jobs down safely
                if (i < *job_count - 1) {
                    memmove(&jobs[i], &jobs[i+1],
                            (*job_count - i - 1) * sizeof(Job));
                }
                (*job_count)--;

                // Stay at the same index since jobs shifted
                i--;
            }
        }
    }

    if (result == -1 && errno != ECHILD) {
        perror("waitpid");
    }
}

void sigint_handler(int sig, int* fg_pid) {
    if (*fg_pid > 0) {
        kill(-(*fg_pid), SIGINT);  // send to process group
    }
}

void sigtstp_handler(int sig, int* fg_pid, Job* jobs, int* job_count, int* next_job_id, char* fg_cmdline) {
    if (*fg_pid > 0) {
        // Stop the foreground process group
        kill(-(*fg_pid), SIGTSTP);

        // Find a free slot in jobs[]
        if (*job_count < 64) {
            jobs[*job_count].pid = fg_pid;
            jobs[*job_count].job_id = (*next_job_id)++;
            jobs[*job_count].state = STOPPED;

            // Copy the command line (must be set when you launched fg job)
            strncpy(jobs[*job_count].cmdline, fg_cmdline,
                    sizeof(jobs[*job_count].cmdline) - 1);
            jobs[*job_count].cmdline[sizeof(jobs[*job_count].cmdline) - 1] = '\0';

            (*job_count)++;

            printf("[%d] Stopped %s\n",
                   jobs[*job_count - 1].job_id,
                   jobs[*job_count - 1].cmdline);
            fflush(stdout);
        }

        // Reset fg_pid so shell knows no fg job
        *fg_pid = -1;
    }
}

void install_handlers() {
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
}

////////////// LLM Generated Code Ends ///////////////
