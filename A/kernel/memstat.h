// Fix: include types.h for uint definition
#ifndef MEMSTAT_H
#define MEMSTAT_H
#include "types.h"

#define MAX_PAGES_INFO 128 // Max pages to report per syscall

// Page states
#define UNMAPPED 0 
#define RESIDENT 1 
#define SWAPPED  2

struct page_stat {
  uint va;    // virtual address
  int state;  // UNMAPPED, RESIDENT, SWAPPED
  int is_dirty; // 0 or 1
  int seq;     // FIFO sequence number
  int swap_slot; // swap slot number, -1 if not swapped
};

struct proc_mem_stat {
  int pid;
  int num_pages_total;     
  int num_resident_pages; 
  int num_swapped_pages;   
  int next_fifo_seq;       
  struct page_stat pages[MAX_PAGES_INFO];
};

#endif // MEMSTAT_H
