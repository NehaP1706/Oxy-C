#include "shell.h"

int cmpfunc(const void *a, const void *b) {
    return strcmp(*(char **)a, *(char **)b);
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