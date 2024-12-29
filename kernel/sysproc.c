#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
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
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
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

#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  uint64 baseaddr, abits_addr;
  int pgt_len, rv;
  unsigned int abits = 0x00;
  struct proc *cur_proc = myproc();
  argaddr(0, &baseaddr);
  argaddr(2, &abits_addr);
  argint(1, &pgt_len);

  if(pgt_len > 32){
    return -1;
  }

  for(int i = 0; i < pgt_len; i++, baseaddr += PGSIZE){
    pte_t *pte = walk(cur_proc->pagetable, baseaddr, 0);
    if((*pte) & PTE_A){
      abits |= (1 << i);
      printf("trig=%d\n", i);
      *pte &= ~PTE_A;
    }
  }

  rv = copyout(cur_proc->pagetable, abits_addr, (char*) &abits, sizeof(abits));
  if(rv != 0){
    printf("copyout failed.\n");
    return -1;
  }
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
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
