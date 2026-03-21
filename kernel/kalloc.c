// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

void init_percpu_kmemlock(){
  for(int i=0;i<NCPU;i++){
    char kmem_lockname[16];
    snprintf(kmem_lockname, sizeof(kmem_lockname), "kmem%d", i);
    initlock(&cpus[i].kmem.lock, kmem_lockname);
  }
}

void init_percpu_kmemfreelist(){
  for(int i=0;i<NCPU;i++){
    cpus[i].kmem.freelist = (struct cpurun *) 0;
  }
}
void
kinit()
{
  // initlock(&kmem.lock, "kmem");
  init_percpu_kmemlock();
  init_percpu_kmemfreelist();
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  int cpuid = 0;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    cpuid = (cpuid + 1) % NCPU;
    // no need to do lock here, when calling freerange, xv6_OS is still initializing.
    struct cpu *cpu = &cpus[cpuid];
    struct cpurun *r = (struct cpurun *) p;
    r->next = cpu->kmem.freelist;
    cpu->kmem.freelist = r;
  }
}

// borrow free mem block from other cpus
void* kmem_cpus_deploy(){
  struct cpurun *r;

  for(int cpuid=0;cpuid<NCPU;cpuid++){
    struct cpu *cpu = &cpus[cpuid];
    acquire(&cpu->kmem.lock);
    r = cpu->kmem.freelist;
    if(r){
      cpu->kmem.freelist = r->next;
      release(&cpu->kmem.lock);
      break;
    }else{
      release(&cpu->kmem.lock);
    }
  }
  return (void *) r;

}
// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct cpurun *r;
  struct cpu *cpu = mycpu();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct cpurun*)pa;

  acquire(&cpu->kmem.lock);
  r->next = cpu->kmem.freelist;
  cpu->kmem.freelist = r;
  release(&cpu->kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct cpurun *r;
  struct cpu *cpu = mycpu();

  acquire(&cpu->kmem.lock);
  r = cpu->kmem.freelist;
  if(r)
    cpu->kmem.freelist = r->next;
  release(&cpu->kmem.lock);

  if(!r)
    r = (struct cpurun *) kmem_cpus_deploy();

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  else{
    printf("kalloc: allocate failed!\n");
  }
  return (void*)r;
}
