#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
  if (argc != 3) {
    return 0;
  }

  char buf[256];
  sprintf(buf, "/home/qyzhang/Projects/GraduationProject/code/KTableFS/build/mount/%s", argv[2]);
  if (!strcmp(argv[1], "create")) {
    printf("create %s\n", buf);
    creat(buf, 0666);
  } else if (!strcmp(argv[1], "open")) {
    printf("open %s\n", buf);
    open(buf, O_RDWR);
  }
  return 0;
}