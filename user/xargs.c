#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(2, "no enough arg provided\n");
    exit(1);
  }

  char *argv_pass[MAXARG];
  char *prog_name = argv[1];
  argv_pass[0] = prog_name;

  for (int i = 1; i < argc - 1; i++) {
    argv_pass[i] = argv[i + 1];
  }

  char buf[512], *p;
  p = buf;

  while (read(0, p++, 1) == 1) {
    if (*p != '\n') {
      continue;
    }

    // 读到了换行符
    *p = 0;  // 添加字符串结束标识
    argv_pass[argc - 1] = buf;
    if (fork() == 0) {
      exec(prog_name, argv_pass);
      fprintf(2, "exec error");
      exit(1);
    } else {
      wait(0);
      p = buf;
    }
  }
  exit(0);
}
