
// Fill proc_mem_stat for the calling process
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "memstat.h"

uint64 sys_memstat(void) {
  uint64 info_addr;
  argaddr(0, &info_addr);
  struct proc *p = myproc();
  struct proc_mem_stat stat;
  stat.pid = p->pid;
  stat.num_pages_total = (p->sz + PGSIZE - 1) / PGSIZE;
  stat.num_resident_pages = p->resident_count;
  stat.num_swapped_pages = p->swap_count;
  stat.next_fifo_seq = p->next_fifo_seq;
  int count = 0;
  for(int i=0; i<p->resident_count && count<MAX_PAGES_INFO; i++) {
    stat.pages[count].va = p->resident_pages[i];
    stat.pages[count].state = RESIDENT;
    stat.pages[count].is_dirty = p->page_dirty[i];
    stat.pages[count].seq = p->page_seq[i];
    stat.pages[count].swap_slot = -1;
    count++;
  }
  for(int i=0; i<SWAP_SLOTS && count<MAX_PAGES_INFO; i++) {
    if(p->swap_slots_used[i]) {
      stat.pages[count].va = p->swap_va[i];
      stat.pages[count].state = SWAPPED;
      stat.pages[count].is_dirty = 1;
      stat.pages[count].seq = -1;
      stat.pages[count].swap_slot = i;
      count++;
    }
  }
  // Unmapped pages
  for(uint64 va=0; va<p->sz && count<MAX_PAGES_INFO; va+=PGSIZE) {
    int mapped = 0;
    for(int i=0; i<p->resident_count; i++) if(p->resident_pages[i] == va) mapped=1;
  for(int i=0; i<SWAP_SLOTS; i++) if(p->swap_slots_used[i] && p->swap_va[i]==va) mapped=1;
    if(!mapped) {
      stat.pages[count].va = va;
      stat.pages[count].state = UNMAPPED;
      stat.pages[count].is_dirty = 0;
      stat.pages[count].seq = -1;
      stat.pages[count].swap_slot = -1;
      count++;
    }
  }
  if(copyout(p->pagetable, info_addr, (char*)&stat, sizeof(stat)) < 0) return -1;
  return 0;
}

#ifndef PGSIZE
#define PGSIZE 4096
#endif
// If PGSIZE is still undefined, define it here again
#ifndef PGSIZE
#define PGSIZE 4096
#endif
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
// Fix: include riscv.h for PGSIZE
#include "riscv.h"
#include "vm.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;
  struct proc *p = myproc();

  argint(0, &n);
  argint(1, &t);
  addr = p->sz;

  addr = p->sz;
  // Just update size, don't allocate pages yet
  p->sz += n;

  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
