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
  
  for (int i = 0; i < argc; i++) {
    argv_pass[i]
  }
}
