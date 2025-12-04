# Lab: page tables

## Speed up system calls

在进程的内存空间中映射一个从 USYSCALL 开始的只读页，并且设置为用户进程可访问，存储进程的 pid，加速用户进程访问。

#### 步骤1: 在创建进程页表时，为虚拟地址 USYSCALL 添加映射

- 使用 `kalloc` 申请一页空闲空间（kernel/proc.c：226-232）
    ```c
     uint64 free_page = (uint64)kalloc();
     if (free_page == 0) {
       uvmunmap(pagetable, TRAPFRAME, 1, 0);
       uvmunmap(pagetable, TRAMPOLINE, 1, 0);
       uvmfree(pagetable, 0);
       return 0;
     }
    ```

- 使用申请到的页表为 USYSCALL 建立映射（kernel/proc.c：234-241）
    ```c
      if(mappages(pagetable, USYSCALL, PGSIZE,
                  free_page, PTE_R | PTE_U) < 0){
        kfree((void *)free_page);
        uvmunmap(pagetable, TRAPFRAME, 1, 0);
        uvmunmap(pagetable, TRAMPOLINE, 1, 0);
        uvmfree(pagetable, 0);
        return 0;
      }
    ```

#### 步骤2：在建立完页表后，将 pid 存储到对应的地址

- 获取 USYSCALL 在进程页表中对应的物理地址（kernel/proc.c：147-152）
    ```c
      uint64 pa = walkaddr(p->pagetable, USYSCALL);  // get phasical address
      if (pa == 0) {
        freeproc(p);
        release(&p->lock);
        return 0;
      }
    ```

- 将 pid 写入，内核空间内物理地址可以直接等于虚拟地址（kernel/proc.c：155-156）
    ```c
      struct usyscall *uc = (struct usyscall *)pa;
      uc->pid = p->pid;
    ```

#### 步骤3：在释放页表的时候手动释放 USYSCALL 的映射

必须同时释放 `kalloc` 创建的空闲物理页，指定 `uvmunmap` 的参数 `do_free` 为 1，以调用 `kfree` 释放物理页（kernel/proc.c：260）

```c
  uvmunmap(pagetable, USYSCALL, 1, 1);  // unmap USYSCALL）
```

## Print a page table

递归打印 page table 中的 PTE（kernel/vm.c：439-463）

```c
void
vmprint_helper(pagetable_t pagetable, int depth)
{
  if (depth > 3) {
    return;
  }
  for (int i = 0; i < 512; i++) {
    pte_t pte = pagetable[i];
    if (pte & PTE_V) {
      for (int j = 0; j < depth - 1; j++) {
        printf(".. ");
      }
      uint64 child = PTE2PA(pte);
      printf("..%d: pte %p pa %p\n", i, pte, child);
      vmprint_helper((pagetable_t)child, depth + 1);
    }
  }
}

void
vmprint(pagetable_t pagetable)
{
  printf("page table %p\n", pagetable);
  vmprint_helper(pagetable, 1);
}
```

## Detecting which pages have been accessed

实现 `pgaccess` 系统调用，由于在 `kernel/syscall.h` 已经定义 `SYS_pgaccess` 对应的索引，在 `kernel/syscall.c` 中已经定义所有到 `sys_pgaccess` 的映射，所以只用在 `kernel/sysproc.c` 中实现 `sys_pgaccess`

- 使用 `argaddr` 和 `argint` 获取传入参数（kernel/sysproc.c：84-94）
    ```c
      uint64 va;  // first arg: virtual address of the first user page to check
      if (argaddr(0, &va) < 0)
        return -1;

      int n;  // second arg: the number of pages to check
      if (argint(1, &n) < 0 || n > 64)
        return -1;

      uint64 ua;  // third arg: address of buffer to store the result
      if (argaddr(2, &ua) < 0)
        return -1;
    ```

- 使用 `walk` 依次获取虚拟地址对于的 PTE，检查 `PTE_A`，并把结果复制到用户空间（kernel/sysproc.c：96-117）
    ```c
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
    ```
