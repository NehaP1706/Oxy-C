#ifndef XV6_VM_H_LAZY_HELPERS
#define XV6_VM_H_LAZY_HELPERS

#include "types.h"
#include "memlayout.h"
#include "riscv.h"

typedef uint64 pte_t;
typedef pte_t* pagetable_t;

int alloc_user_page(pagetable_t pagetable, uint64 va);
int lazy_copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len);

#endif
#define SBRK_EAGER 1
#define SBRK_LAZY  2
