#include "shell.h"

char last_dir[1024] = "";   // global, tracks the previous dir

void handle_hop(char* cwd, char **argv, char* shell_home) {
    char temp[1024];

    for (int i = 1; argv[i] != NULL; i++) {
        char *target = argv[i];

        if (strcmp(target, "~") == 0 || strcmp(target, shell_home) == 0) {
            strcpy(temp, cwd);
            if (chdir(shell_home) == 0) {
                getcwd(cwd, 1024);   // <-- real path /home/neha
                strcpy(last_dir, temp);
            } else {
                perror("hop");
            }
        }
        else if (strcmp(target, "-") == 0) {
            // hop to previous
            if (strlen(last_dir) == 0) {
                continue; // nothing stored yet
            }
            strcpy(temp, cwd);
            if (chdir(last_dir) == 0) {
                getcwd(cwd, 1024);
                printf("%s\n", cwd);
                strcpy(last_dir, temp);         // swap
                if (strcmp(cwd, shell_home) == 0)
                    strcpy(cwd, "~");
            } else {
                perror("hop");
            }
        }
        else {
            // hop to normal directory
            strcpy(temp, cwd);
            if (chdir(target) == 0) {
                getcwd(cwd, 1024);
                strcpy(last_dir, temp);         // store actual previous dir
                if (strcmp(cwd, shell_home) == 0)
                    strcpy(cwd, "~");
            } else {
                perror("hop");
            }
        }
    }
}
