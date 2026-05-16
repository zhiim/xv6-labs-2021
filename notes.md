## Uthread: switching between threads

`user/uthread.c` 中的测试用例定义了三个不同线程需要执行的函数，在 `main` 中首先创建三个线程，然后执行 `thread_schedule` 切换到第一个可执行线程。

三个线程会不断打印计数，没打印一次计数，就调用 `thread_yield` 切换其他线程，所以三个线程会依次执行，直到所有线程都执行完毕。

为了实现上述过程，我们需要实现

- 线程调度：切换到另一个线程
- 线程创建：创建线程，当调度到当前线程时执行线程对应的函数

### 1. 线程调度

循环查找可运行的线程

```c
for(int i = 0; i < MAX_THREAD; i++){
  if(t >= all_thread + MAX_THREAD)
    t = all_thread;
  if(t->state == RUNNABLE) {
    next_thread = t;
    break;
  }
  t = t + 1;
}
```

切换线程（切换上下文）

```c
thread_switch((uint64) &t->context, (uint64) &next_thread->context);
```

caller-saved 寄存器如果后续还需要使用，编译器会让 caller 在 `thread_switch` 函数调用前自动保存在栈中，在通过改变 `sp` 寄存器完成栈切换后会自动读出新值。

所以 `thread_switch` 只需要保存 callee-saved 寄存器，并且确保 `sp` 寄存器被替换实现栈替换，此外还需要改变 `ra` 替换返回地址。

```c
struct thread_context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};
```

```asm
	.text

	/*
         * save the old thread's registers,
         * restore the new thread's registers.
         */

	.globl thread_switch
thread_switch:
	/* YOUR CODE HERE */
        sd ra, 0(a0)
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        ld ra, 0(a1)
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)

	ret    /* return to ra */
```

### 2. 线程创建

在创建线程的时候，需要让 CPU 切换到当前写成时执行对应的函数。所以在 `thread_create` 函数里面，要把 `ra` 设置为函数的地址。

```c
t->context.ra = (uint64) func;
```

此外，创建线程的时候还要为线程设置栈。

```c
t->context.sp = (uint64) t->stack + STACK_SIZE;
```

## Using threads

### 1. 多线程读写 hash 表

使用 `pthread_create` 创建多个线程，并且为每个线程指定需要运行的函数

```c
for(int i = 0; i < nthread; i++) {
  assert(pthread_create(&tha[i], NULL, get_thread, (void *) (long) i) == 0);
}
```

使用 `pthread_join` 同步等待所有线程任务执行结束，并回收资源

```c
for(int i = 0; i < nthread; i++) {
  assert(pthread_join(tha[i], &value) == 0);
}
```

### 2. 添加锁防止进程冲突

多个线程同时向哈希表写数据，可能导致数据写入的前后顺序冲突。为了解决这个问题，可以用锁保护数据写过程的原子性。

代码中所有读操作是在写操作完成之后才执行的，所以读操作不需要锁保护，否则需用在读中使用锁防止读写顺序混乱。

在代码中哈希表总过有 `NBUCKET` 个桶，这几个桶相互独立，可以同时读写，所以可以给每个桶设置一个独立的锁，这样不同的桶可以同时写，加快速度。

首先创建锁

```c
pthread_mutex_t lock[NBUCKET];
```

初始化锁（C 语言不可在 global level 执行函数，所以需要放在 main 里）

```c
for (int i = 0; i < NBUCKET; i++)
  pthread_mutex_init(&lock[i], NULL);
```

在写操作前后用锁保护

```c
pthread_mutex_lock(&lock[i]);
for (e = table[i]; e != 0; e = e->next) {
  if (e->key == key)
    break;
}
if(e){
  // update the existing key.
  e->value = value;
} else {
  // the new is new.
  insert(key, value, &table[i], table[i]);
}
pthread_mutex_unlock(&lock[i]);
```

## Barrier

实现一个 barrier，在还有线程没有执行 barrier 之前，阻塞前面的线程。此时前面的线程可以用条件锁暂时让出 CPU，并释放锁。

```
static void 
barrier()
{
  // 使用锁保护对 bstate 修改的原子性
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread++;
  // 如果还有线程没有到达 barrier
  if (bstate.nthread < nthread) {
    // 当前线程可以先让出 CPU，释放锁，并等待唤醒
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  }
  // 如果当前线程已经是最后一个线程
  else {
    bstate.round++;
    // 重置状态
    bstate.nthread = 0;
    // 唤醒其他休眠的线程，被唤醒的线程会重新获得锁
    pthread_cond_broadcast(&bstate.barrier_cond);
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
}
```
