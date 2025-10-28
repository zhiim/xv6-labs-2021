#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc <= 1) {
    // print out error message
    fprintf(2, "sleep: sleep for [num] number of ticks\n");
    exit(1);
  }

  // convert string to int
  int n_time = atoi(argv[1]);

  // call `sleep` system call
  sleep(n_time);

  exit(0);
}
