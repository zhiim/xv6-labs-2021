# Lab Notes

## RISC-V assembly

`user/call.c` 汇编源码

```asm
0000000000000000 <g>:
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int g(int x) {
   # xv6 的栈向下生长，sp-16 为函数调用分配出 16 字节的空间
   0:	1141                	addi	sp,sp,-16
   # 将分配的栈中的第二个 8 字节用来保存 s0，也就是 caller 的栈指针（栈帧的底部）
   2:	e422                	sd	s0,8(sp)
   # s0 指向当前函数的栈帧的底部
   4:	0800                	addi	s0,sp,16
  return x+3;
}
   # 执行算术运算
   6:	250d                	addiw	a0,a0,3
   # 读取之前保存的 caller 的栈指针
   8:	6422                	ld	s0,8(sp)
   # 复原 sp
   a:	0141                	addi	sp,sp,16
   c:	8082                	ret

000000000000000e <f>:

int f(int x) {
   e:	1141                	addi	sp,sp,-16
  10:	e422                	sd	s0,8(sp)
  12:	0800                	addi	s0,sp,16
  return g(x);
}
  # 直接数值计算
  14:	250d                	addiw	a0,a0,3
  16:	6422                	ld	s0,8(sp)
  18:	0141                	addi	sp,sp,16
  1a:	8082                	ret

000000000000001c <main>:

void main(void) {
  1c:	1141                	addi	sp,sp,-16
  # 需要调用其他函数，此时需将返回地址 ra 保存
  1e:	e406                	sd	ra,8(sp)
  20:	e022                	sd	s0,0(sp)
  22:	0800                	addi	s0,sp,16
  printf("%d %d\n", f(8)+1, 13);
  # 将传入参数保存到对于寄存器
  24:	4635                	li	a2,13
  26:	45b1                	li	a1,12
  # a0 存储程序计数器 pc 的值
  28:	00000517          	auipc	a0,0x0
  # a0 + 1968 指向了格式化后的字符串，作为第一个传入参数
  2c:	7b050513          	addi	a0,a0,1968 # 7d8 <malloc+0xea>
  # ra 存储当前的程序计数器
  30:	00000097          	auipc	ra,0x0
  # ra 存储 pc + 4（下一条指令），pc 指向 ra + 1536（printf 函数）
  34:	600080e7          	jalr	1536(ra) # 630 <printf>
  exit(0);
  38:	4501                	li	a0,0
  3a:	00000097          	auipc	ra,0x0
  3e:	27e080e7          	jalr	638(ra) # 2b8 <exit>
```

## Backtrace

寄存器 `s0` 保存了当前函数的栈指针，使用 `r_fp` 读取 `s0` 寄存器得到当前执行的函数的栈指针 `fp`
```c
  uint64 fp = r_fp();
```

每个线程只有一个内核栈，并且是单页大小。栈向下生长，所以函数调用返回时，栈指针向上，如果 `fp < PGROUNDUP(fp)` 那么函数返回到达顶点
```c
  uint64 up_addr = PGROUNDUP(fp);
  uint64 down_addr = PGROUNDDOWN(fp);

  while (fp < up_addr && fp >= down_addr) {
```

每个栈帧中（fp - 16）对应的寄存器是 caller 的栈指针，栈指针保存了栈的地址。首先将 `fp - 16` 映射成一个指针，然后解引用取出地址 `fp - 16` 保存的值（也就是栈指针保存的值）
```c
    fp = *(uint64*)(fp - 16);
```

## Alarm

### 系统调用

`user/user.h` 添加 system call 的调用接口

`kernel/syscall.h` 添加 SYS_ 的索引值定义

`user/usys.pl` 通过脚本为每个用户空间的 system call 生成入口，将 system call 索引加载到 `a7` 寄存器，然后执行 `ecall` 进入系统调用

`kernel/syscall.c` 中 `syscall` 函数读取 `a7` 寄存器的值，并执行对应的系统调用函数 `sys_xxx`

在系统调用函数内，通过 `argint`、`argaddr` 等获取 system call 传入的参数

### sys_sigalarm

修改 `kernel/proc.h`，在 `struct proc` 内添加 `alarm_interval` 用于记录 alarm 周期，`alarm_handler` 用于记录 handler 的地址，`alarm_interval_passed` 记录已经经过的周期

`kernel/proc.c` 里 `allocproc` 初始化 `alarm_interval_passed` 为 0

`sys_sigalarm` 中分别使用 `argint` 和 `argaddr` 获取 system call `sigalarm` 的传输参数，并保存在 `proc` 结构体中

`kernel/trap.c` 中的 `usertrap` 用于判断何种trap，其中计数器中断对应 `which_dev == 2`，每次增加 `alarm_interval_passed` 直到达到 `alarm_interval`。将 `alarm_handler` 的地址（用户空间的地址，内核空间无法直接使用）赋给 `trapframe->epc`，当 trap 返回用户空间时执行 handler

### sys_sigreturn

`trapframe->epc` 被替换成了 handler 的地址，trap 返回用户空间后会执行 handler 函数，handler 会改变各种用于寄存器，当 handler 执行完之后已经无法恢复 trap 之前的状态

在 handler 之前需要保存用户空间的状态，并在 handler 之后恢复（sigreturn）

在 proc 结构体中定义用于保存状态的成员变量，并在 usertrap 的计时器中断部分进行保存。handler 结束之前执行 sigreturn 恢复到计时器中断之前

### 防止中断干扰

alarm handler 是用户态程序，在其执行的时候不应该有新的周期 alarm 产生，防止保存的状态寄存器被覆写

应该设置变量指示是否处于 handler 中，handler 执行完在 sigreturn 中重置
