#include "user.h"
#include "kernel/memstat.h"

void print_memstat() {
    struct proc_mem_stat stat;
    if (memstat(&stat) < 0) {
        printf("memstat syscall failed\n");
        return;
    }
    printf("pid=%d total=%d resident=%d swapped=%d next_seq=%d\n",
        stat.pid, stat.num_pages_total, stat.num_resident_pages, stat.num_swapped_pages, stat.next_fifo_seq);
    for (int i = 0; i < MAX_PAGES_INFO; i++) {
        if (stat.pages[i].state == UNMAPPED) continue;
        printf("va=0x%x state=%s dirty=%d seq=%d slot=%d\n",
            stat.pages[i].va,
            stat.pages[i].state == RESIDENT ? "RESIDENT" : (stat.pages[i].state == SWAPPED ? "SWAPPED" : "UNMAPPED"),
            stat.pages[i].is_dirty,
            stat.pages[i].seq,
            stat.pages[i].swap_slot);
    }
}

int main() {
    printf("Allocating heap pages...\n");
    char *heap = sbrk(4096 * 8); // allocate 8 pages
    for (int i = 0; i < 8; i++) {
        heap[i * 4096] = i; // touch each page to trigger fault
    }
    print_memstat();
    printf("Accessing stack...\n");
    print_memstat();
    printf("Test complete.\n");
    exit(0);
}
