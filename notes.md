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
