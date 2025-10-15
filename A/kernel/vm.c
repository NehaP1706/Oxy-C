/////////////////////// LLM Generated Code Begins ///////////////////////

#include "types.h"
#include "proc.h"
#include "vm.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "elf.h"

// Return permission bits appropriate for the segment containing va.
int
segment_perms(struct proc *p, uint64 va)
{
  va = PGROUNDDOWN(va);
  if (va >= p->text_start && va < p->text_end)
    return PTE_U | PTE_R | PTE_X;
  if (va >= p->data_start && va < p->data_end)
    return PTE_U | PTE_R | PTE_W;
  if (va >= p->heap_start && va < p->sz)
    return PTE_U | PTE_R | PTE_W;
  if (va >= (p->stack_top - USERSTACK*PGSIZE) && va < p->stack_top)
    return PTE_U | PTE_R | PTE_W;
  // Default: user-readable
  return PTE_U | PTE_R;
}

// Read a page-sized slot from the process's swap file into a newly
// allocated kernel page and return the buffer, or 0 on error.
char *
swap_read_page(struct proc *p, int slot)
{
  if (!p || slot < 0 || slot >= SWAP_SLOTS) return 0;
  if (!p->swap_file || !p->swap_file->ip) {
    if (p->swapfilename[0]) {
      struct inode *sip = namei(p->swapfilename);
      if (!sip) return 0;
      if (!p->swap_file) p->swap_file = filealloc();
      if (!p->swap_file) { iunlockput(sip); return 0; }
      p->swap_file->type = FD_INODE;
      p->swap_file->ip = sip;
      p->swap_file->readable = 1;
      p->swap_file->writable = 1;
    } else {
      return 0;
    }
  }

  char *buf = kalloc();
  if (!buf) return 0;

  int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
  int read = 0;
  while (read < PGSIZE) {
    int to_read = PGSIZE - read;
    if (to_read > max) to_read = max;
    begin_op();
    ilock(p->swap_file->ip);
    int r = readi(p->swap_file->ip, 0, (uint64)(buf + read), slot * PGSIZE + read, to_read);
    iunlock(p->swap_file->ip);
    end_op();
    if (r <= 0) {
      kfree(buf);
      return 0;
    }
    read += r;
  }
  return buf;
}

// Page-fault handler: determine cause/source and resolve the fault.
int
handle_page_fault(struct proc *p, uint64 va, int access_type)
{
  // access_type: 0=read/load, 1=write/store, 2=exec
  char *access_str = (access_type == 0) ? "read" : (access_type == 1) ? "write" : "exec";
  // Remember the original faulting address for logging, then align va to page base
  uint64 reported_va = va;
  va = PGROUNDDOWN(va);
  // Print the PAGEFAULT prefix; following prints append cause and newline.
  printf("[pid %d] PAGEFAULT va=0x%lx access=%s", p->pid, reported_va, access_str);

  // Determine cause/source
  int cause = -1; // 0=heap, 1=stack, 2=exec, 3=swap, 4=invalid
  // If this VA was previously swapped out, handle it as a SWAP fault first.
  for (int i = 0; i < SWAP_SLOTS; i++) {
    if (p->swap_slots_used[i] && p->swap_va[i] == va) {
      printf(" cause=swap\n");
      cause = 3;
      break;
    }
  }

  if (cause != 3) {
    if (va >= p->text_start && va < p->text_end) {
      printf(" cause=exec\n");
      cause = 2;
    } else if (va >= p->data_start && va < p->data_end) {
      printf(" cause=exec\n");
      cause = 2;
    } else if (va >= p->heap_start && va < p->sz) {
      printf(" cause=heap\n");
      cause = 0;
    } else if (va >= (p->stack_top - 2*USERSTACK*PGSIZE) && va < p->stack_top) {
      printf(" cause=stack\n");
      cause = 1;
      //printf("[pid %d] STACKFAULT va=0x%lx sp=0x%lx\n", p->pid, va, p->trapframe ? p->trapframe->sp : 0);
    } else {
      printf(" cause=invalid\n");
      printf("[pid %d] KILL invalid-access va=0x%lx access=%s\n", p->pid, va, access_str);
      return 0;
    }
  }

  void *mem = 0;

  if (p->resident_count >= SWAP_SLOTS) {
    // Resident pages full, evict first before allocating
    printf("[pid %d] MEMFULL\n", p->pid);
    
    int victim_idx = -1, min_seq = 0x7fffffff;
    for (int i = 0; i < p->resident_count; i++) {
      // Skip empty entries.
      if (p->resident_pages[i] == 0) continue;
      if (p->page_seq[i] < min_seq) {
        min_seq = p->page_seq[i];
        victim_idx = i;
      }
    }
    if (victim_idx == -1) {
      printf("[pid %d] KILL no victim found\n", p->pid);
      return 0;
    }

    uint64 victim_va = p->resident_pages[victim_idx];
    int dirty = p->page_dirty[victim_idx];
    printf("[pid %d] VICTIM va=0x%lx seq=%d algo=FIFO\n", p->pid, victim_va, min_seq);
    printf("[pid %d] EVICT  va=0x%lx state=%s\n", p->pid, victim_va, dirty ? "dirty" : "clean");

    // If the page is clean and has a valid executable/data backing,
    // we can discard it (no need to write to swap). Otherwise, write
    // it out to the per-process swap file.
    int has_exec_backing = 0;
    if (!dirty) {
      if ((victim_va >= p->text_start && victim_va < p->text_end) ||
          (victim_va >= p->data_start && victim_va < p->data_end)) {
        has_exec_backing = 1;
      }
    }

    if (!dirty && has_exec_backing) {
      // Clean page with executable/data backing: discard the page.
      printf("[pid %d] DISCARD va=0x%lx\n", p->pid, victim_va);
      // Unmap and free the physical page
      uvmunmap(p->pagetable, victim_va, 1, 1);
    } else {
      // Need to write page to swap (dirty or clean without backing)

      // Find swap slot
      int slot = -1;
      for (int i = 0; i < SWAP_SLOTS; i++) {
        if (!p->swap_slots_used[i]) { slot = i; break; }
      }
      if (slot == -1) {
        // No swap slots available: fatal
        printf("[pid %d] SWAPFULL\n", p->pid);
        printf("[pid %d] KILL swap-exhausted\n", p->pid);
        return 0;
      }

      // Tentatively reserve the slot; if write fails we'll release it.
      p->swap_slots_used[slot] = 1;
      p->swap_va[slot] = victim_va;
      p->swap_count++;
      // Debug: print reservation info
      if (p->swap_file && p->swap_file->ip)
        printf("[pid %d] SWAPOUT-Debug: slot=%d va=0x%lx swapfile=%s ip->inum=%d ip->size=%d\n",
               p->pid, slot, victim_va, p->swapfilename, p->swap_file->ip->inum, p->swap_file->ip->size);

      pte_t *pte = walk(p->pagetable, victim_va, 0);
      if (!pte || !(*pte & PTE_V)) {
        printf("[pid %d] SWAPOUT failed: page invalid\n", p->pid);
        // undo reservation
        p->swap_slots_used[slot] = 0;
        p->swap_va[slot] = 0;
        p->swap_count--;
        return 0;
      }
      char *pa = (char*)PTE2PA(*pte);

      // Ensure swap file/inode is valid; attempt to reopen if needed.
      if (!p->swap_file || !p->swap_file->ip || p->swap_file->ip->ref < 1) {
        if (p->swapfilename[0]) {
          struct inode *sip = namei(p->swapfilename);
          if (sip) {
            // namei returns unlocked inode; do not iunlock here
            if (!p->swap_file) p->swap_file = filealloc();
            if (p->swap_file) {
              p->swap_file->type = FD_INODE;
              p->swap_file->ip = sip;
              p->swap_file->readable = 1;
              p->swap_file->writable = 1;
            }
          } else {
            printf("[pid %d] SWAPOUT: cannot reopen swap file %s\n", p->pid, p->swapfilename);
            // undo reservation
            p->swap_slots_used[slot] = 0;
            p->swap_va[slot] = 0;
            p->swap_count--;
            return 0;
          }
        } else {
          printf("[pid %d] SWAPOUT: no swap filename\n", p->pid);
          // undo reservation
          p->swap_slots_used[slot] = 0;
          p->swap_va[slot] = 0;
          p->swap_count--;
          return 0;
        }
      }

      if (!p->swap_file || !p->swap_file->ip) {
        printf("[pid %d] SWAPOUT: missing swap_file for slot=%d\n", p->pid, slot);
        // undo reservation
        p->swap_slots_used[slot] = 0;
        p->swap_va[slot] = 0;
        p->swap_count--;
        return 0;
      }

  // Allocate a kernel buffer and copy the page contents into it.
      char *buf = kalloc();
      if (!buf) {
        printf("[pid %d] SWAPOUT: kalloc failed for slot=%d\n", p->pid, slot);
        // undo reservation
        p->swap_slots_used[slot] = 0;
        p->swap_va[slot] = 0;
        p->swap_count--;
        return 0;
      }
      memmove(buf, (void*)pa, PGSIZE);

      // Write page in chunks to avoid exceeding journal transaction limits.

      // Write page in chunks to avoid exceeding journal transaction limits.
      int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
      int written = 0;
      // Allocate a zero buffer for on-demand preallocation.
        while (written < PGSIZE) {
          int to_write = PGSIZE - written;
          if (to_write > max) to_write = max;

          begin_op();
          ilock(p->swap_file->ip);
          int r = writei(p->swap_file->ip, 0, (uint64)(buf + written), slot * PGSIZE + written, to_write);
          iunlock(p->swap_file->ip);
          end_op();
          if (r != to_write) {
            // Log details for debugging write failures
            int inode_num = p->swap_file && p->swap_file->ip ? p->swap_file->ip->inum : -1;
            int inode_size = p->swap_file && p->swap_file->ip ? p->swap_file->ip->size : -1;
            printf("[pid %d] SWAPOUT write failed slot=%d wrote=%d expected=%d inode=%d size=%d\n",
                   p->pid, slot, r, to_write, inode_num, inode_size);
            // Treat any write failure as swap exhaustion/fatal per spec
            p->swap_slots_used[slot] = 0;
            p->swap_va[slot] = 0;
            p->swap_count--;
            kfree(buf);
            printf("[pid %d] SWAPFULL\n", p->pid);
            printf("[pid %d] KILL swap-exhausted\n", p->pid);
            return 0;
          }
          written += r;
        }
      kfree(buf);
        printf("[pid %d] SWAPOUT va=0x%lx slot=%d\n", p->pid, victim_va, slot);
      uvmunmap(p->pagetable, victim_va, 1, 1);
    }

    // Remove victim from resident set
    for (int i = victim_idx; i < p->resident_count-1; i++) {
      p->resident_pages[i] = p->resident_pages[i+1];
      p->page_seq[i] = p->page_seq[i+1];
      p->page_dirty[i] = p->page_dirty[i+1];
    }
    // Clear the now-unused tail slot to avoid stale entries.
    int old_count = p->resident_count;
    p->resident_count--;
    if (p->resident_count >= 0 && p->resident_count < old_count) {
      p->resident_pages[p->resident_count] = 0;
      p->page_seq[p->resident_count] = 0;
      p->page_dirty[p->resident_count] = 0;
    }
  }

  // Now allocate a fresh page. If out of frames, perform FIFO eviction
  mem = kalloc();
  if (mem == 0) {
    // Out of frames: choose a victim from this process's resident set
    printf("[pid %d] MEMFULL\n", p->pid);
    int victim_idx = -1, min_seq = 0x7fffffff;
    for (int i = 0; i < p->resident_count; i++) {
      if (p->resident_pages[i] == 0) continue;
      if (p->page_seq[i] < min_seq) { min_seq = p->page_seq[i]; victim_idx = i; }
    }
    if (victim_idx == -1) {
      printf("[pid %d] KILL no victim found\n", p->pid);
      return 0;
    }
    uint64 victim_va = p->resident_pages[victim_idx];
    int dirty = p->page_dirty[victim_idx];
    printf("[pid %d] VICTIM va=0x%lx seq=%d algo=FIFO\n", p->pid, victim_va, min_seq);
    printf("[pid %d] EVICT  va=0x%lx state=%s\n", p->pid, victim_va, dirty ? "dirty" : "clean");

    int has_exec_backing = 0;
    if (!dirty) {
      if ((victim_va >= p->text_start && victim_va < p->text_end) ||
          (victim_va >= p->data_start && victim_va < p->data_end)) {
        has_exec_backing = 1;
      }
    }

    if (!dirty && has_exec_backing) {
      printf("[pid %d] DISCARD va=0x%lx\n", p->pid, victim_va);
      uvmunmap(p->pagetable, victim_va, 1, 1);
    } else {
  int slot = -1;
  for (int i = 0; i < SWAP_SLOTS; i++) if (!p->swap_slots_used[i]) { slot = i; break; }
      if (slot == -1) {
        printf("[pid %d] SWAPFULL\n", p->pid);
        printf("[pid %d] KILL swap-exhausted\n", p->pid);
        return 0;
      }
      p->swap_slots_used[slot] = 1;
      p->swap_va[slot] = victim_va;
      p->swap_count++;
      pte_t *pte = walk(p->pagetable, victim_va, 0);
      if (!pte || !(*pte & PTE_V)) {
        p->swap_slots_used[slot] = 0;
        p->swap_va[slot] = 0;
        p->swap_count--;
        return 0;
      }
      char *pa = (char*)PTE2PA(*pte);
      char *buf = kalloc();
      if (!buf) {
        p->swap_slots_used[slot] = 0;
        p->swap_va[slot] = 0;
        p->swap_count--;
        return 0;
      }
      memmove(buf, (void*)pa, PGSIZE);
      int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
      int written = 0;
      while (written < PGSIZE) {
        int to_write = PGSIZE - written;
        if (to_write > max) to_write = max;
        begin_op();
        ilock(p->swap_file->ip);
        int r = writei(p->swap_file->ip, 0, (uint64)(buf + written), slot * PGSIZE + written, to_write);
        iunlock(p->swap_file->ip);
        end_op();
        if (r != to_write) {
          int inode_num = p->swap_file && p->swap_file->ip ? p->swap_file->ip->inum : -1;
          int inode_size = p->swap_file && p->swap_file->ip ? p->swap_file->ip->size : -1;
          printf("[pid %d] SWAPOUT write failed slot=%d wrote=%d expected=%d inode=%d size=%d\n",
                 p->pid, slot, r, to_write, inode_num, inode_size);
          p->swap_slots_used[slot] = 0;
          p->swap_va[slot] = 0;
          p->swap_count--;
          kfree(buf);
          printf("[pid %d] SWAPFULL\n", p->pid);
          printf("[pid %d] KILL swap-exhausted\n", p->pid);
          return 0;
        }
        written += r;
      }
      kfree(buf);
      printf("[pid %d] SWAPOUT va=0x%lx slot=%d\n", p->pid, victim_va, slot);
      uvmunmap(p->pagetable, victim_va, 1, 1);
    }

    // Remove victim from resident set
    for (int i = victim_idx; i < p->resident_count-1; i++) {
      p->resident_pages[i] = p->resident_pages[i+1];
      p->page_seq[i] = p->page_seq[i+1];
      p->page_dirty[i] = p->page_dirty[i+1];
    }
    int old_count = p->resident_count;
    p->resident_count--;
    if (p->resident_count >= 0 && p->resident_count < old_count) {
      p->resident_pages[p->resident_count] = 0;
      p->page_seq[p->resident_count] = 0;
      p->page_dirty[p->resident_count] = 0;
    }

    // Now retry allocation
    mem = kalloc();
    if (mem == 0) {
      printf("[pid %d] Allocation failed even after eviction\n", p->pid);
      return 0;
    }
  }


  int perm = 0;
  //printf("[CHECK] Checking stack range: va=0x%lx stack_top=0x%lx stack_base=0x%lx\n", va, p->stack_top, p->stack_top - USERSTACK*PGSIZE);
  if (cause == 0 || cause == 1) {
    if (cause == 1) { // stack
      //printf("[STACK] PAGEFAULT on stack va=0x%lx (sp=0x%lx)\n", va, p->trapframe ? p->trapframe->sp : 0);
      uint64 sp = p->trapframe ? p->trapframe->sp : 0;
      if (va <= sp && sp < va + PGSIZE) {
        //printf("[STACK] PAGEFAULT at stack page containing SP: va=0x%lx, sp=0x%lx\n", va, sp);
      }
    }

    // Heap or stack: allocate zero-filled page
    memset(mem, 0, PGSIZE);
    perm = segment_perms(p, va);
    if (mappages(p->pagetable, va, PGSIZE, (uint64)mem, perm) != 0) {
      kfree(mem);
      return 0;
    }
    sfence_vma();
    printf("[pid %d] ALLOC va=0x%lx\n", p->pid, va);
    if (va == (p->trapframe ? p->trapframe->epc : 0)) {
      printf("[pid %d] MAPPED ENTRYPOINT va=0x%lx\n", p->pid, va);
    }
    if (va == (p->trapframe ? p->trapframe->sp : 0)) {
      printf("[pid %d] MAPPED STACK va=0x%lx\n", p->pid, va);
    }
  } else if (cause == 2) {
    // Text/data: load correct page contents from executable
    struct inode *ip = namei(p->exec_path);
    if (!ip) {
      printf("[pid %d] KILL cannot open executable for demand paging\n", p->pid);
      kfree(mem);
      return 0;
    }
    ilock(ip);
    // Find the correct program header
    struct elfhdr elf;
    if (readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf)) {
      iunlockput(ip);
      kfree(mem);
      return 0;
    }
    struct proghdr ph;
    int found = 0;
    for (int i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
      if (readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph)) continue;
      if (ph.type != ELF_PROG_LOAD) continue;
      if (va >= ph.vaddr && va < ph.vaddr + ph.memsz) {
        // Debug: print ELF segment info and file offset calculation
        // printf("[pid %d] ELF SEGMENT: ph.off=0x%lx ph.vaddr=0x%lx ph.filesz=0x%lx ph.memsz=0x%lx\n", p->pid, ph.off, ph.vaddr, ph.filesz, ph.memsz);
        uint file_offset = ph.off + (va - ph.vaddr);
        // printf("[pid %d] PAGEFAULT: va=0x%lx file_offset=0x%x\n", p->pid, va, file_offset);
        uint to_read = PGSIZE;
        if (file_offset + to_read > ph.off + ph.filesz)
          to_read = ph.off + ph.filesz > file_offset ? (ph.off + ph.filesz - file_offset) : 0;
        // printf("[pid %d] PAGEFAULT: to_read=0x%x\n", p->pid, to_read);
        memset(mem, 0, PGSIZE);
        if (to_read > 0) {
          if (readi(ip, 0, (uint64)mem, file_offset, to_read) != to_read) {
            iunlockput(ip);
            kfree(mem);
            return 0;
          }
        }
        found = 1;
        break;
      }
    }
    iunlockput(ip);
    if (!found) {
      printf("[pid %d] KILL cannot find segment for va=0x%lx\n", p->pid, va);
      kfree(mem);
      return 0;
    }
    perm = segment_perms(p, va);
    if (mappages(p->pagetable, va, PGSIZE, (uint64)mem, perm) != 0) {
      kfree(mem);
      return 0;
    }
    sfence_vma();
    // Debug: print first 16 bytes of loaded page
    printf("[pid %d] LOADEXEC va=0x%lx\n", p->pid, va);
    // for (int dbi = 0; dbi < 16; dbi++) {
    //   printf("%x ", ((unsigned char*)mem)[dbi]);
    // }
    //printf("\n");
    // Print 16 bytes at entry point offset in this page
    // if (va <= (p->trapframe ? p->trapframe->epc : 0) && (p->trapframe ? p->trapframe->epc : 0) < va + PGSIZE) {
    //   uint64 epc_off = (p->trapframe ? p->trapframe->epc : 0) - va;
    //   //printf("[pid %d] ENTRYPOINT bytes at 0x%lx: ", p->pid, p->trapframe ? p->trapframe->epc : 0);
    //   for (int dbi = 0; dbi < 16 && epc_off + dbi < PGSIZE; dbi++) {
    //     //printf("%x ", ((unsigned char*)mem)[epc_off + dbi]);
    //   }
    //   printf("\n");
    // }
    // if (va == (p->trapframe ? p->trapframe->epc : 0)) {
    //   printf("[pid %d] MAPPED ENTRYPOINT va=0x%lx\n", p->pid, va);
    // }
    // if (va == (p->trapframe ? p->trapframe->sp : 0)) {
    //   printf("[pid %d] MAPPED STACK va=0x%lx\n", p->pid, va);
    // }
  } else if (cause == 3) {
    int slot = -1;
    for (int i = 0; i < SWAP_SLOTS; i++) {
      if (p->swap_slots_used[i] && p->swap_va[i] == va) { slot = i; break; }
    }
    if (slot == -1) return 0;

    char *mem = swap_read_page(p, slot);
    if (mem == 0) return 0;

    int perm = segment_perms(p, va) | PTE_V;
    if (mappages(p->pagetable, va, PGSIZE, (uint64)mem, perm) != 0) {
      kfree(mem);
      return 0;
    }
    sfence_vma();
    // Only clear the swap slot after successful mapping
    p->swap_slots_used[slot] = 0;
    p->swap_va[slot] = 0;
    p->swap_count--;
    printf("[pid %d] SWAPIN va=0x%lx slot=%d\n", p->pid, va, slot);
  }

  // Add to resident set
  int idx = p->resident_count;
  if (idx < SWAP_SLOTS) {
    if (va != 0) {
      p->resident_pages[idx] = va;
      p->page_seq[idx] = p->next_fifo_seq++;
      // Mark page dirty if the faulting access was a write; otherwise
      // initialize as clean.
      p->page_dirty[idx] = (access_type == 1) ? 1 : 0;
      p->resident_count++;
      printf("[pid %d] RESIDENT va=0x%lx seq=%d\n", p->pid, va, p->page_seq[idx]);
    }
  }
  return 1;
}

/////////////////////// LLM Generated Code Ends ///////////////////////

#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t) kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // allocate and map a kernel stack for each process.
  proc_mapstacks(kpgtbl);
  
  return kpgtbl;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// Initialize the kernel_pagetable, shared by all CPUs.
void
kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// Switch the current CPU's h/w page table register to
// the kernel's page table, and enable paging.
void
kvminithart()
{
  // wait for any previous writes to the page table memory to finish.
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  // flush stale entries from the TLB.
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
  if(!alloc || (pagetable = (pte_t*)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_V) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa.
// va and size MUST be page-aligned.
// Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("mappages: va not aligned");

  if((size % PGSIZE) != 0)
    panic("mappages: size not aligned");

  if(size == 0)
    panic("mappages: size");
  
  a = va;
  last = va + size - PGSIZE;
  for(;;){
    if((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. It's OK if the mappings don't exist.
// Optionally free the physical memory.
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0) // leaf page table entry allocated?
      continue;   
    if((*pte & PTE_V) == 0)  // has physical page been allocated?
      continue;
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

// Allocate PTEs and physical memory to grow a process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  kfree((void*)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      continue;   // page table entry hasn't been allocated
    if((*pte & PTE_V) == 0)
      continue;   // physical page hasn't been allocated
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
    sfence_vma();
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    if(va0 >= MAXVA)
      return -1;
  
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) {
      if (!handle_page_fault(myproc(), va0, 1))
        return -1;
      pa0 = walkaddr(pagetable, va0);
      if (pa0 == 0)
        return -1;
    }

    pte = walk(pagetable, va0, 0);
    // forbid copyout over read-only user text pages.
    if((*pte & PTE_W) == 0)
      return -1;
      
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    // If this write modified a resident page, mark it dirty in the process's
    // resident tracking so eviction will write it to swap.
    struct proc *p = myproc();
    if (p) {
      for (int i = 0; i < p->resident_count; i++) {
        if (p->resident_pages[i] == va0) {
          p->page_dirty[i] = 1;
          break;
        }
      }
    }

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) {
      if((pa0 = vmfault(pagetable, va0, 0)) == 0) {
        return -1;
      }
    }
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}

// allocate and map user memory if process is referencing a page
// that was lazily allocated in sys_sbrk().
// returns 0 if va is invalid or already mapped, or if
// out of physical memory, and physical address if successful.
uint64
vmfault(pagetable_t pagetable, uint64 va, int read)
{
  uint64 mem;
  struct proc *p = myproc();

  if (va >= p->sz)
    return 0;
  va = PGROUNDDOWN(va);
  if(ismapped(pagetable, va)) {
    return 0;
  }
  mem = (uint64) kalloc();
  if(mem == 0)
    return 0;
  memset((void *) mem, 0, PGSIZE);
  if (mappages(p->pagetable, va, PGSIZE, mem, PTE_W|PTE_U|PTE_R) != 0) {
    kfree((void *)mem);
    return 0;
  }
  return mem;
}

int
ismapped(pagetable_t pagetable, uint64 va)
{
  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0) {
    return 0;
  }
  if (*pte & PTE_V){
    return 1;
  }
  return 0;
}
