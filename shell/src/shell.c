#include "shell.h"

int main()
{
    char user[1024];
    char systemName[1024];

    ////////////// LLM Generated Code Begins ///////////////

    strcpy(user, getenv("USER"));
    
    if (gethostname(systemName, 1024) != 0)
    {
        printf("Environment Variable (HOSTNAME) error.");
        exit(1);
    }

    //printf("USER: %s\n", user);
    //printf("SYSTEMNAME: %s\n", systemName);

    ////////////// LLM Generated Code Ends ///////////////

    while (1)
    {      
        char* dir_name = make_init_dir_name();

        char* display = make_init_display(user, systemName, dir_name);

        printf("%s", display);
        free(display);

        char* input = malloc(1024);
        scanf("%1023[^\n]", input);

        scanf("%*[^\n]");
        scanf("%*c");

        free(input);
        free(dir_name);
    }

    return 0;
}