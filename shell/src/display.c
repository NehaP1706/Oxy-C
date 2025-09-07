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
    char* display = (char*) malloc (sizeof(char)*2048);

    strcpy(display, "<");
    strcat(display, user);
    strcat(display, "@");
    strcat(display, systemName);
    strcat(display, ":");
    strcat(display, dir_name);
    strcat(display, "> ");

    return display;
}
