#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
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
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
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


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
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
  // lab pgtbl: your code here.
  uint64 va;  // first arg: virtual address of the first user page to check
  if (argaddr(0, &va) < 0)
    return -1;

  int n;  // second arg: the number of pages to check
  if (argint(1, &n) < 0 || n > 64)
    return -1;

  uint64 ua;  // third arg: address of buffer to store the result
  if (argaddr(2, &ua) < 0)
    return -1;

  struct proc *p = myproc();
  pagetable_t pagetable = p->pagetable;

  uint64 bitmask = 0;

  pte_t *pte;
  uint64 addr;
  // check pages
  for (int i = 0; i < n; i++) {
    addr = va + i * PGSIZE;
    if ((pte = walk(pagetable, addr, 0)) == 0) {
      return -1;
    }
    if (*pte & PTE_A) {
      *pte &= (~PTE_A);  // clear PTE_A is it's set
      bitmask |= (1L << i);  // mask corresponding bit
    }
  }

  // move bitmask to user space
  if ((copyout(pagetable, ua, (char *)&bitmask, 8)) == -1)
    return -1;

  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
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
