## `fork()` 分析

- `allocproc()` 分配新进程
- `uvmcopy()` 将父进程的 pagetable 拷贝到子进程
- 拷贝 trapframe
- 遍历打开的文件，将计数加一
- `safestrcpy()` 拷贝进程名称

## 原 `uvmcopy()` 分析

```c
  for(i = 0; i < sz; i += PGSIZE){
    // 通过 walk 找到当前地址对应的 PTE（最后一级）
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    // PTE_V 标识当前 PTE 是否存在
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    // 得到 PTE 的物理地址
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    // 使用 kalloc 分配一页内存
    if((mem = kalloc()) == 0)
      goto err;
    // 使用 memmove 将 pa 处的内容拷贝到 mem
    memmove(mem, (char*)pa, PGSIZE);
    // 创建 PTE
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
```
## COW 实现

### 1. 改变 `fork()` 时拷贝内存的逻辑

修改 `uvmcopy()`，不为子进程分配新内存，而是将子进程的 PTE 指向父进程的页

```c
    /* 
     * 将 PTE flags 中的 PTE_W 清除，这样后续访问的时候会出发 page fault
     * 此外添加 PTE_COW 位用于判断 page fault 是否是由 COW 机制产生
     * PTE_COW 在 kernel/riscv.h 中定义
     */
    flags = (flags & (~PTE_W)) | PTE_COW;
    /*
     * PA2PTE 获取当前物理地址的无 flags PTE （只有 PPN）
     * 与 flags 与运算为 PTE 设置 flags
     */
    *pte = PA2PTE(pa) | flags;
    // 直接将虚拟地址 i 与物理地址 pa (父进程的内存物理地址) 之间建立映射
    if(mappages(new, i, PGSIZE, (uint64)pa, flags) != 0){
      goto err;
    }
```

### 2. 为每个页设置引用计数

使用一个数组记录每个页的引用数量

```c
struct {
  struct spinlock lock;
  int count[PHYSTOP / PGSIZE];  // 内存最大地址 PHYSTOP
} pg_ref;
```

`kalloc()` 分配页的时候将计数设置为1

```c
  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk

    acquire(&pg_ref.lock);
    pg_ref.count[(uint64)r / PGSIZE] = 1;
    release(&pg_ref.lock);
  }
```

`uvmcopy()` 中子进程引用时计数加 1

```c
    kchangecnt((uint64)pa);
```

`kfree()` 释放页的时候将计数减 1，并且只在计数为 0 的时候释放

```c
  acquire(&pg_ref.lock);
  int count = --pg_ref.count[(uint64)pa / PGSIZE];
  release(&pg_ref.lock);

  if (count > 0)
    return;
```

### 3. 在 `usertrap()` 中处理 cow page fault

page fault 对应的 scause 寄存器值为 15

```c
  } else if (r_scause() == 15) {
    // page fault on write

    pagetable_t pgtable = myproc()->pagetable;

    uint64 va;
    uint64 pa;
    pte_t *pte;
    uint flags;
    char *mem;

    /*
     * stval 寄存器记录了发生 page fault 的虚拟地址
     * 我们需要对页操作，所以需要使用 PGROUNDDOWN 将虚拟地址取到页起始地址
     */
    va = PGROUNDDOWN(r_stval());
    if (va >= MAXVA) {
      myproc()->killed = 1;
      exit(-1);
    }

    // 使用 walk 找到虚拟地址对应的物理地址
    if ((pte = walk(pgtable, va, 0)) == 0) {
      myproc()->killed = 1;
      exit(-1);
    }
    if ((*pte & PTE_V) == 0) {
      myproc()->killed = 1;
      exit(-1);
    }
    pa = PTE2PA(*pte);

    flags = PTE_FLAGS(*pte);
    // if not cow page fault, exit
    if ((flags & PTE_COW) == 0) {
      myproc()->killed = 1;
      exit(-1);
    }
    // 设置 PTE_W 并清除 PTE_COW
    flags = (flags | PTE_W) & (~PTE_COW);

    // 分配新内存页
    if ((mem = kalloc()) == 0)
      exit(-1);
    // 将父进程的页内容拷贝到新分配的内存
    memmove(mem, (char*)pa, PGSIZE);

    /*
     * mappages 不会映射已经映射过的虚拟地址
     * 所以这里直接手动建立 pte 到新内存页的映射，并设置 flags
     * NOTE: 不需要重置父进程的 flags，如果父进程也遇到 cow page fault 会自己处理
     * 如果在子进程里面直接修改父进程的 pte 会增加复杂度，因为无法确定父子进程哪个先进入 cow page fault
     */
    *pte = PA2PTE(mem) | flags;

    /*
     * kfree 在子进程中的作用只是将页引用计数减 1
     * 在父进程中页引用计数将减为0，原页被释放
     */
    kfree((void*)pa);
  }
```

### 防止 `copyout` 报错

`copyout()` 用于将内核空间的数据拷贝到用户空间，需要访问用户页表，但是运行在内核空间，所以用于页表的 PTE_W 和 PTE_COW 对其不生效，不会触发 page fault trap

在 `copyout()` 内发生数据拷贝 `memmove()` 之前，需要判断 PTE 是否存在 PTE_COW，并执行写时复制

```c
    va0 = PGROUNDDOWN(dstva);
    if (va0 > MAXVA)
      return -1;
    pa0 = walkaddr(pagetable, va0);
    if ((pte = walk(pagetable, va0, 0)) == 0) {
      return -1;
    }
    if ((*pte & PTE_V) == 0) {
      return -1;
    }
    flags = PTE_FLAGS(*pte);
    // 如果当前 PTE 存在 PTE_COW 位，执行 COW
    if ((flags & PTE_COW)) {
      flags = (flags | PTE_W) & (~PTE_COW);
      if ((mem = kalloc()) == 0)
        return -1;
      memmove(mem, (char*)pa0, PGSIZE);
      *pte = PA2PTE(mem) | flags;
      kfree((void*)pa0);
      // 使用新内存替换旧内存
      pa0 = (uint64)mem;
    }
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);
```
