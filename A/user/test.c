#include "kernel/types.h"
#include "user/user.h"

#define ALLOC_PAGES 1100  // More than resident set size
#define PAGE_SIZE 4096

int main() {
    printf("Starting swap stress test...\n");

    for (int i = 0; i < ALLOC_PAGES; i++) {
        char *p = sbrk(PAGE_SIZE);
        if (p <= (char*)0) {
            printf("sbrk failed at page %d\n", i);
            break;
        }
        for (int j = 0; j < PAGE_SIZE; j++) {
          p[j] = (char)j;
        }
        p[0] = i; // write to allocate and mark dirty
        printf("Allocated and wrote to page %d at %p\n", i, p);
    }

    printf("Allocation finished, sleeping...\n");
    pause(1000);

    exit(0);
}
