#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"
#include <stdint.h>

// Local itoa implementation (no string.h)
void itoa(int value, char *str, int base) {
  int i = 0, sign = 0;
  if (value == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return;
  }
  if (value < 0 && base == 10) {
    sign = 1;
    value = -value;
  }
  while (value != 0) {
    int rem = value % base;
    str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
    value = value / base;
  }
  if (sign) str[i++] = '-';
  str[i] = '\0';
  // Reverse
  int start = 0, end = i - 1;
  while (start < end) {
    char tmp = str[start];
    str[start] = str[end];
    str[end] = tmp;
    start++;
    end--;
  }
}
// Fix: include file.h for struct file
#include "file.h"
// Fix: include fs.h for T_FILE
#include "fs.h"
// For create()
struct inode *create(char *path, short type, short major, short minor);
// If T_FILE is still undefined, define it here
#ifndef T_FILE
#define T_FILE 2
#endif

//static int loadseg(pagetable_t, uint64, struct inode *, uint, uint);

// map ELF permissions to PTE permission bits.
int flags2perm(int flags)
{
    int perm = 0;
    if(flags & 0x1)
      perm = PTE_X;
    if(flags & 0x2)
      perm |= PTE_W;
    return perm;
}

//
// the implementation of the exec() system call
//
// Enhanced: log every exec call and its arguments
int kexec(char *path, char **argv)
{
  //printf("[exec] ENTRY: kexec called for %s\n", path);
  
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG+1], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();

  begin_op();

  // Open the executable file.
  if((ip = namei(path)) == 0){
    //printf("[exec] ERROR: file not found: %s\n", path);
    end_op();
    return -1;
  }
  ilock(ip);

  // Read the ELF header.
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf)) {
    //printf("[exec] ERROR: failed to read ELF header for %s\n", path);
    goto bad;
  }

  // Is this really an ELF file?
  if(elf.magic != ELF_MAGIC) {
    //printf("[exec] ERROR: not a valid ELF file: %s\n", path);
    goto bad;
  }

  if((pagetable = proc_pagetable(p)) == 0) {
    //printf("[exec] ERROR: failed to create pagetable for %s\n", path);
    goto bad;
  }

  // Lazy mapping: record text/data segment ranges, don't allocate/load pages yet.
  uint64 text_start = (uint64)-1, text_end = 0, data_start = (uint64)-1, data_end = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)) {
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph)) {
      //printf("[exec] ERROR: failed to read program header for %s\n", path);
      goto bad;
    }
    // Debug: print program header fields to diagnose missing segments
    printf("[exec] ph[%d]: type=%d flags=0x%x off=0x%lx vaddr=0x%lx filesz=0x%lx memsz=0x%lx\n",
           i, ph.type, ph.flags, ph.off, ph.vaddr, ph.filesz, ph.memsz);
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz) {
      //printf("[exec] ERROR: memsz < filesz in program header for %s\n", path);
      goto bad;
    }
    if(ph.vaddr + ph.memsz < ph.vaddr) {
      //printf("[exec] ERROR: vaddr + memsz overflow in program header for %s\n", path);
      goto bad;
    }
    if(ph.vaddr % PGSIZE != 0) {
      //printf("[exec] ERROR: vaddr not page aligned in program header for %s\n", path);
      goto bad;
    }
    // Record text/data segment ranges
   if(ph.flags & 0x1) { // Executable
    if(ph.vaddr < text_start) text_start = ph.vaddr;
    if(ph.vaddr + ph.memsz > text_end) text_end = ph.vaddr + ph.memsz;
    } 
    if(ph.flags & 0x2) { // Writable (data)
      if(ph.vaddr < data_start) data_start = ph.vaddr;
      if(ph.vaddr + ph.memsz > data_end) data_end = ph.vaddr + ph.memsz;
    }
    // Do not allocate or load pages here
    sz = ph.vaddr + ph.memsz > sz ? ph.vaddr + ph.memsz : sz;
  }

  if(text_start == (uint64)-1) text_start = text_end = 0;
  if(data_start == (uint64)-1) data_start = data_end = 0;
  
  // Save lazy mapping info in proc
  p->text_start = text_start;
  p->text_end = text_end;
  p->data_start = data_start;
  p->data_end = data_end;
  p->heap_start = sz;
  p->stack_top = PGROUNDUP(sz) + (USERSTACK+1)*PGSIZE;
  //printf("[EXEC CHECK] stack_top=0x%lx USERSTACK=%d PGSIZE=%d stack_base=0x%lx\n", p->stack_top, USERSTACK, PGSIZE, p->stack_top - USERSTACK*PGSIZE);
  // Create per-process swap file
  char swapname[64];
  safestrcpy(swapname, "/pgswp", 8);
  char pidstr[8];
  itoa(p->pid, pidstr, 10);
  /* append pid immediately after "/pgswp"; 
    "/pgswp" has length 6, so start at offset 6. 
    Using +7 left the previous '\0' in place and printed only "/pgswp". */
  safestrcpy(swapname+6, pidstr, 8);

  // Create per-process swap file. Add debug prints to detect hangs.
  //printf("[exec] creating swap file %s\n", swapname);
  //begin_op();
  iunlockput(ip);      
  end_op();
  ip = 0;

  begin_op(); 
  struct inode *swapip = create(swapname, T_FILE, 0, 0);
  //printf("[exec] create returned swapip=0x%lx\n", (uint64)swapip);

  if(swapip == 0) {
    goto bad;
  }

  // Keep the inode referenced; the file structure will hold and
  // later close/unlink it. Do not iput() here.
  iunlock(swapip);
  end_op();

  printf("[exec] swapfile created OK: %s (inum=%d)\n", swapname, swapip->inum);

  //ilock(ip);

  p->swap_file = filealloc();
  p->swap_file->type = FD_INODE;
  p->swap_file->ip = swapip;
  p->swap_file->readable = 1;
  p->swap_file->writable = 1;
  p->swap_count = 0;
  safestrcpy(p->swapfilename, swapname, sizeof(p->swapfilename));
  for(int si=0; si<SWAP_SLOTS; si++) {
    p->swap_slots_used[si] = 0;
    p->swap_va[si] = 0;
  }
  // Preallocate a modest number of swap pages to reserve space and
  // fail early if the filesystem is low on space. Use chunked writes
  // to respect the journaling transaction limits.
  // Reduce preallocation to avoid exhausting the filesystem on small images.
  // If you still run out of swap, increase fs.img size instead of preallocating.
  int prealloc_pages = 0; // disabled: avoid filling small fs images
  if (prealloc_pages > 0) {
    char *z = kalloc();
    if (z) {
      memset(z, 0, PGSIZE);
      int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
      for (int sp = 0; sp < prealloc_pages; sp++) {
        int written = 0;
        while (written < PGSIZE) {
          int to_write = PGSIZE - written;
          if (to_write > max) to_write = max;
          begin_op();
          ilock(swapip);
          int r = writei(swapip, 0, (uint64)(z + written), sp * PGSIZE + written, to_write);
          iunlock(swapip);
          end_op();
          if (r != to_write) {
            // Preallocation failed; likely filesystem is full. Log and stop trying.
            printf("[exec] swap prealloc failed at page %d wrote=%d expected=%d (stop prealloc)\n", sp, r, to_write);
            sp = prealloc_pages; // break outer loop
            break;
          }
          written += r;
        }
      }
      kfree(z);
    }
    printf("[exec] swap prealloc done (or skipped)\n");
  } else {
    printf("[exec] swap prealloc disabled (prealloc_pages=0)\n");
  }
  // Reset resident set and FIFO counters to remove stale entries from
  // previous image. Exec replaces the process memory, so resident
  // bookkeeping must be cleared.
  p->resident_count = 0;
  p->next_fifo_seq = 0;
  for (int ri = 0; ri < SWAP_SLOTS; ri++) {
    p->resident_pages[ri] = 0;
    p->page_seq[ri] = 0;
    p->page_dirty[ri] = 0;
  }
  //end_op();
  // Log lazy mapping
  printf("[pid %d] INIT-LAZYMAP text=[0x%lx,0x%lx) data=[0x%lx,0x%lx) heap_start=0x%lx stack_top=0x%lx\n", p->pid, text_start, text_end, data_start, data_end, p->heap_start, p->stack_top);
  printf("[exec] before uvmalloc for stack sz=%lu\n", sz);
  // iunlockput(ip);
  // end_op();
  // ip = 0;
  
  p = myproc();
  uint64 oldsz = p->sz;

  // Allocate some pages at the next page boundary.
  // Make the first inaccessible as a stack guard.
  // Use the rest as the user stack.
  sz = PGROUNDUP(sz);
  uint64 sz1;
  // Map stack pages writable so exec can copy argv/ustack into them.
  // User writes will still trap later if pages are remapped or protections change.
  if((sz1 = uvmalloc(pagetable, sz, sz + (USERSTACK+1)*PGSIZE, PTE_W)) == 0) {
    //printf("[exec] ERROR: uvmalloc failed for stack for %s\n", path);
    goto bad;
  }
  printf("[exec] uvmalloc stack OK new sz=%lu\n", sz1);
  sz = sz1;
  sp = sz;
  p->sz = sz;
  stackbase = sp - USERSTACK*PGSIZE;
  uvmclear(pagetable, stackbase - PGSIZE);

  // Copy argument strings into new stack, remember their
  // addresses in ustack[].
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    //sp -= sp % 16; // riscv sp must be 16-byte aligned
    if(sp < stackbase)
      goto bad;
    //printf("[exec] argv[%ld] user stack addr=0x%lx, string='%s'\n", argc, sp, argv[argc]);
    if(copyout(pagetable, sp, argv[argc], (strlen(argv[argc]) + 1)) < 0) {
      //printf("[exec] ERROR: lazy_copyout failed for argv[%ld] for %s\n", argc, path);
      goto bad;
    }

    printf("[exec] copied argv[%ld] to 0x%lx ('%s')\n", argc, sp, argv[argc]);

    // After copying each argument string, dump physical mem
    // printf("[DEBUG] Copied argv[%ld] to va=0x%lx\n", argc, sp);
    // uint64 pa = walkaddr(pagetable, sp);
    // if (pa) {
    //   printf("[DEBUG] Arg string at va=0x%lx pa=0x%lx: ", sp, pa);
    //   for (int k = 0; k < strlen(argv[argc]) + 1; k++) {
    //     printf("%x ", ((unsigned char*)pa)[k]);
    //   }
    //   printf("\n");
    // }
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push a copy of ustack[], the array of argv[] pointers.
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0) {
    //printf("[exec] ERROR: lazy_copyout failed for ustack for %s\n", path);
    goto bad;
  }

  // a0 and a1 contain arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.

  // printf("[DEBUG] ustack array copied to va=0x%lx\n", sp);
  // uint64 pa = walkaddr(pagetable, sp);
  // if (pa) {
  //   printf("[DEBUG] ustack array at pa=0x%lx: ", pa);
  //   for (int i = 0; i < argc; i++) {
  //     printf("0x%lx ", ((uint64*)pa)[i]);
  //   }
  //   printf("\n");
  // }

  // Ensure all stack pages containing argv strings and ustack array are mapped
  // for (int i = 0; i < argc; i++) {
  //   uint64 va = PGROUNDDOWN(ustack[i]);
  //   pte_t *pte = walk(pagetable, va, 0);
  //   if (pte && (*pte & PTE_V)) {
  //     printf("[DEBUG] Stack page for argv[%d] at va=0x%lx mapped, perms=0x%lx\n", i, va, PTE_FLAGS(*pte));
  //   } else {
  //     printf("[BUG] Stack page for argv[%d] at va=0x%lx NOT mapped!\n", i, va);
  //   }
  // }
  
  // uint64 ustack_va = PGROUNDDOWN(sp);
  // pte_t *pte = walk(pagetable, ustack_va, 0);
  // if (pte && (*pte & PTE_V)) {
  //   printf("[DEBUG] Stack page for ustack array at va=0x%lx mapped, perms=0x%lx\n", ustack_va, PTE_FLAGS(*pte));
  // } else {
  //   printf("[BUG] Stack page for ustack array at va=0x%lx NOT mapped!\n", ustack_va);
  // }


  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));
  //printf("[exec] proc name set to %s\n", p->name);
  // Save full executable path for demand paging
  safestrcpy(p->exec_path, path, sizeof(p->exec_path));

  // pa = walkaddr(pagetable, 0x3fc0);
  // if (pa) {
  //   printf("[DEBUG] Before switch, pa=0x%lx, first 16 bytes: ", pa);
  //   for (int i = 0; i < 16; i++) printf("%x ", ((unsigned char*)pa)[i]);
  //   printf("\n");
  // }

  // Commit to the user image.
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = (sz);
  //printf("[pid %d] ELF entry point: 0x%lx\n", p->pid, elf.entry);
  
  p->trapframe->epc = elf.entry;  // initial program counter

  uint64 argv_addr = sp;
  //sp -= sp%16; // or sp -= sizeof(uint64);
  p->trapframe->a1 = argv_addr;
  p->trapframe->sp = sp;
  //printf("[DEBUG] Final trapframe setup: epc=0x%lx, sp=0x%lx, a1=0x%lx\n", elf.entry, p->trapframe->sp, p->trapframe->a1);

  // for (int i = 0; i < argc; i++) {
  //   uint64 va = ustack[i];
  //   pte_t *pte = walk(p->pagetable, va, 0);
  //   if (!pte || !(*pte & PTE_V)) {
  //     printf("[BUG] (after switch) Stack page at 0x%lx not mapped!\n", va);
  //   } else {
  //     printf("[OK] (after switch) Stack page at 0x%lx mapped to pa=0x%lx\n", va, PTE2PA(*pte));
  //     uint64 pa = walkaddr(p->pagetable, va);
  //     printf("[DEBUG] (after switch) At va=0x%lx pa=0x%lx, first 16 bytes: ", va, pa);
  //     for (int j = 0; j < 16; j++) printf("%x ", ((unsigned char*)pa)[j]);
  //     printf("\n");
  //   }
  // }

  proc_freepagetable(oldpagetable, oldsz);

  // pa = walkaddr(pagetable, 0x3000);
  // if (pa) {
  //   printf("[DEBUG] Stack page at 0x3000: ");
  //   for (int i = 0; i < 4096; i++) printf("%x ", ((unsigned char*)pa)[i]);
  //   printf("\n");
  // }

  //printf("[exec] SUCCESS: exec completed for %s, entry=0x%lx, sp=0x%lx\n", path, elf.entry, sp);
  return argc; // this ends up in a0, the first argument to main(argc, argv)

bad:
  text_start = 0;
  text_end = 0;
  data_start = 0;
  data_end = 0;
  printf("[exec] ERROR: exec failed for %s\n", path);
  printf("[exec] ELF header: magic=0x%x, entry=0x%lx, phoff=%ld, phnum=%d\n", elf.magic, elf.entry, elf.phoff, elf.phnum);
  printf("[exec] Segments: text=[0x%lx,0x%lx)%s data=[0x%lx,0x%lx)%s heap_start=0x%lx stack_top=0x%lx\n",
    (uint64)text_start, (uint64)text_end, ((uint64)text_start==0||(uint64)text_end==0)?" (MISSING)":"",
    (uint64)data_start, (uint64)data_end, ((uint64)data_start==0||(uint64)data_end==0)?" (MISSING)":"",
    p->heap_start, p->stack_top);
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

// Load an ELF program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
// static int
// loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
// {
//   uint i, n;
//   uint64 pa;

//   for(i = 0; i < sz; i += PGSIZE){
//     pa = walkaddr(pagetable, va + i);
//     if(pa == 0)
//       panic("loadseg: address should exist");
//     if(sz - i < PGSIZE)
//       n = sz - i;
//     else
//       n = PGSIZE;
//     if(readi(ip, 0, (uint64)pa, offset+i, n) != n)
//       return -1;
//   }
  
//   return 0;
// }
