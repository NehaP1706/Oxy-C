#include "shell.h"

int cmpfunc(const void *a, const void *b) {
    return strcmp(*(char **)a, *(char **)b);
}

void assign_pathname(char* pathname, Atomic* at, int* a, int* l, char* dir_name, char* shell_home, char* prev_dir_reveal)
{
    // Default path = current dir (from prompt’s cwd)
    strcpy(pathname, (strcmp(dir_name, "~") == 0 ? shell_home : dir_name));

    int path_count = 0;

    for (int i = 1; at->argv[i] != NULL; i++) {
        if (at->argv[i][0] == '-' && strlen(at->argv[i]) > 1) {
            // Flag group like -a, -l, -al
            for (int j = 1; at->argv[i][j] != '\0'; j++) {
                if (at->argv[i][j] == 'a') {
                    *a = 1;
                } else if (at->argv[i][j] == 'l') {
                    *l = 1;
                }
            }
        } else {
            // It's a pathname (could be "-", "~", ".", "..", or normal)
            path_count++;
            if (path_count > 1) {
                printf("reveal: Invalid Syntax!\n");
                return;   // error
            }

            if (strcmp(at->argv[i], "-") == 0) {
                if (prev_dir_reveal && strlen(prev_dir_reveal) > 0) {
                    strcpy(pathname, prev_dir_reveal);
                } else {
                    printf("No such directory!\n");
                    return;
                }
            } else if (strcmp(at->argv[i], "~") == 0) {
                strcpy(pathname, shell_home);
            } else if (strcmp(at->argv[i], ".") == 0) {
                // current dir
                strcpy(pathname, (strcmp(dir_name, "~") == 0 ? shell_home : dir_name));
            } else if (strcmp(at->argv[i], "..") == 0) {
                // parent dir
                char temp[1024];
                strcpy(temp, (strcmp(dir_name, "~") == 0 ? shell_home : dir_name));
                if (chdir(temp) == 0) {
                    if (chdir("..") == 0) {
                        getcwd(pathname, 1024);
                        // restore working dir back
                        chdir(temp);
                    }
                }
            } else {
                // normal dir name
                strcpy(pathname, at->argv[i]);
            }
        }
    }
    return; // success
}

void handle_reveal(char *path, int a, int l, char *prev_dir_reveal) {
    //printf("FLAGS : a %d l %d\n", a, l);

    DIR *dir = opendir(path);
    if (!dir) {
        perror("reveal");
        return;
    }

    // Save this path for "reveal -"
    strcpy(prev_dir_reveal, path);

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
