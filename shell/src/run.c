#include <stdlib.h>
#include <unistd.h>

int main() {
    execl("./run.out", "./run.out", (char *)NULL);
    return 0;
}
