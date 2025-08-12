#include "shell.h"

char* make_init_dir_name()
{
    char* dir_name = (char*) malloc (sizeof(char)*1000);

    char* pwd = (char*) malloc (sizeof(char)*1024);
    strcpy(pwd, getenv("PWD"));

    if (pwd == NULL)
    {
        printf("Environment Variable (PWD) error.");
        free(pwd);
        exit(1);
    }

    int n = strlen(pwd);

    if (n>5 && pwd[0] == '/' && pwd[1] == 'h' && pwd[2] == 'o' && pwd[3] == 'm' && pwd[4] == 'e' && pwd[5] == '/')
    {
        strcpy(dir_name, "~");   
    }
    else
    {
        strcpy(dir_name, pwd);
    }

    free(pwd);

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