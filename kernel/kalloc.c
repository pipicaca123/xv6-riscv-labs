// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

static int pg_ref_count_table[(PHYSTOP - KERNBASE) / PGSIZE] = {0};
static void init_pg_ref_count_table(){
  for(uint64 i=0; i < (PHYSTOP - KERNBASE) / PGSIZE; i++)
    pg_ref_count_table[i] = 1;
}
int get_pg_count(uint64 pa){
  uint64 page_idx = (pa - KERNBASE) / PGSIZE;
  int reg_cnt = pg_ref_count_table[page_idx];
  return reg_cnt;
}

void set_pg_count(uint64 pa, enum PG_REF_CNT_OPERATION pg_ref_cnt_op){
  uint64 page_idx = (pa - KERNBASE) / PGSIZE;
  // printf("PAGE_TABLE_IDX=%d, max=%d/OP=%d. PA=%p\n", page_idx, (PHYSTOP - KERNBASE) / PGSIZE, pg_ref_cnt_op, pa);
  switch (pg_ref_cnt_op) {
    case PG_REF_CNT_ADD:
      pg_ref_count_table[page_idx]++;
    break;
    case PG_REF_CNT_MINUS:
      pg_ref_count_table[page_idx]--;
    break;
    case PG_REF_CNT_ZERO:
      pg_ref_count_table[page_idx] = 0;
    break;
    default:
      panic("page ref count operation not exist!");
  }
}

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  init_pg_ref_count_table();
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int ref_page_cnt = get_pg_count((uint64) pa);

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if(ref_page_cnt > 1){
    set_pg_count((uint64)pa, PG_REF_CNT_MINUS);
    return;
  }else if(ref_page_cnt < 0){
    printf("BUG!!!ref_page_cnt=%d\n", ref_page_cnt);
  }
  set_pg_count((uint64)pa, PG_REF_CNT_ZERO);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
    set_pg_count((uint64)r, PG_REF_CNT_ADD);
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
