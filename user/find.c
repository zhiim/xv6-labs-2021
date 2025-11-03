#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

/**
 *  格式化并返回当前path对应的文件名
 */
char* fmtname(char *path) {
  char *p;  // 指向文件名称的开始
  p = path + strlen(path);
  while (p >= path && *p != '/') {
    p--;
  }
  p++;  // 指向最后一个/的后一个，即文件名开始

  return p;
}

void find(char* path, char* file_name) {
  struct stat st;
  int fd;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type == T_DEVICE) {
    close(fd);
    return;
  }

  if (st.type == T_FILE) {  // 如果是文件，并且名称相同直接打印，并返回
    if (strcmp(fmtname(path), file_name) == 0) {
      printf("%s\n", path);
    }
    close(fd);
    return;
  }

  struct dirent de;
  char buf[512], *p;  // 用于词义文件路径字符

  // 如果path长度加上/加上文件名称加上0超出buf大小
  if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
    fprintf(2, "path too long\n");
    close(fd);
    return;
  }

  strcpy(buf, path);  // 使用path填充buf
  p = buf + strlen(path);  // p指向0
  *p++ = '/';  // 把0替换为/

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {  // dir其实是dirent seq
    if (de.inum == 0) {
      continue;
    }
    memmove(p, de.name, DIRSIZ);  // 将当前项的名称复制到buf
    p[DIRSIZ] = 0;  // 表示字符串结束
    if (strcmp(fmtname(buf), ".") == 0 || strcmp(fmtname(buf), "..") == 0) {
      continue;
    }
    if (stat(buf, &st) < 0) {
      fprintf(2, "cannot stat %s\n", buf);
      continue;
    }
    find(buf, file_name);
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(2, "no enough arg provided\n");
    exit(1);
  }

  char* dir_path = argv[1];
  char* file_name = argv[2];

  find(dir_path, file_name);

  exit(0);
}
