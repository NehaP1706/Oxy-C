#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

char* make_init_dir_name();
char* make_init_display(char* user, char* systemName, char* dir_name);