#include "shell.h"

int main()
{
    char* user = (char*) malloc (sizeof(char)*1024);
    char* systemName = (char*) malloc (sizeof(char)*1024);

    ////////////// LLM Generated Code Begins ///////////////

    strcpy(user, getenv("USER"));
    
    if (gethostname(systemName, 1024) != 0)
    {
        printf("Environment Variable (HOSTNAME) error.");
        exit(1);
    }

    //printf("USER: %s\n", user);
    //printf("SYSTEMNAME: %s\n", systemName);

    if (!user)
    {
        printf("Environment Variable (USER) error.");
        exit(1);
    }

    ////////////// LLM Generated Code Ends ///////////////

    char* dir_name = make_init_dir_name();
    char* display = make_init_display(user, systemName, dir_name);

    while(1)
    {
        char* input = (char*) malloc (sizeof(char)*1024);

        printf("%s", display);
        int res = scanf("%1023[^\n]", input);

        if (res == -1)
        {
            printf("scanf() error.");
            exit(1);
        }

        scanf("%*[^\n]");  
        scanf("%*c");

        //break;
    }

    free(display);
    free(dir_name);

    free(systemName);
    free(user);

    return 0;
}