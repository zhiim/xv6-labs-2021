#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main()
{
  int p[2];
  pipe(p);  // create pipe
  char buffer = 'a';

  int child_pid = fork();  // create child process

  if (child_pid == 0) {
    // inside child process

    read(p[0], &buffer, 1);
    close(p[0]);

    int pid = getpid();
    printf("%d: received ping\n", pid);

    write(p[1], &buffer, 1);
    close(p[1]);
  } else {
    // inside parent process

    write(p[1], &buffer, 1);
    close(p[1]);

    read(p[0], &buffer, 1);
    close(p[0]);

    int pid = getpid();
    printf("%d: received pong\n", pid);
  }

  exit(0);
}
