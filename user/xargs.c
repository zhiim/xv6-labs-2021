#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

void run_command(char *prog_name, char *argv_pass[]) {
  if (fork() == 0) {
    exec(prog_name, argv_pass);
    fprintf(2, "exec error");
    exit(1);
  } else {
    wait(0);
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(2, "no enough arg provided\n");
    exit(1);
  }

  char *argv_pass[MAXARG];
  char *prog_name = argv[1];
  argv_pass[0] = prog_name;  // 传入exec的第一参数为程序名

  for (int i = 1; i < argc - 1; i++) {
    argv_pass[i] = argv[i + 1];
  }

  char buf[512], *p;
  p = buf;

  while (read(0, p, 1) == 1) {
    if (*p != '\n') {
      p++;
      continue;
    }

    // 读到了换行符
    *p = 0;  // 添加字符串结束标识
    argv_pass[argc - 1] = buf;
    argv_pass[argc] = 0;  // 传入exec的最后一个参数需要为0
    run_command(prog_name, argv_pass);
    p = buf;
  }

  if (p != buf) {  // 说明已经读取了字符，但是不是以\n结尾
    *p = 0;
    argv_pass[argc - 1] = buf;
    argv_pass[argc] = 0;
    run_command(prog_name, argv_pass);
  }

  exit(0);
}
