#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void new_proc(int p[2]) {
  close(p[1]);
  int prime_num = 0;
  if (read(p[0], &prime_num, 4) != 4) {  // 如果没有读取到内容，将会返回0直接退出
    exit(1);
  }
  printf("prime %d\n", prime_num);

  int new_p[2];  // pointer to new pipe
  pipe(new_p);

  int read_num = 0;
  if (fork() == 0) {
    close(p[0]);  // 子进程无需p[0], 由于p[1]已经被父进程关闭，子进程没有继承
    new_proc(new_p);
  } else {
    close(new_p[0]);
    while (read(p[0], &read_num, 4) == 4) {
      if (read_num % prime_num != 0) {
        write(new_p[1], &read_num, 4);
      }
    }
    close(p[0]);
    close(new_p[1]);
    wait(0);
  }
  exit(0);
}

int main() {
  int p[2];
  pipe(p);

  printf("prime 2\n");

  if (fork() == 0) {
    new_proc(p);
  } else {
    close(p[0]);  // 父进程无需使用读端
    for (int i = 2; i <= 35; i++) {
      if (i % 2 != 0) {
        write(p[1], &i, 4);
      }
    }
    close(p[1]);  // 写端不再需要
    wait(0);  // 等待子进程执行结束，防止提前退出子进程变成僵尸进程
  }
  exit(0);
}
