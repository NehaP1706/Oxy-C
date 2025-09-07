#include "shell.h"

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
