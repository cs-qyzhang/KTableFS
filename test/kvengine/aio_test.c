#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "kvengine/aio-wrapper.h"

int main(void) {
  int fd = open("test.txt", O_CREAT | O_RDWR | O_DIRECT, 0644);
  if (fd == -1) {
    printf("open error!\n");
    perror(strerror(errno));
  }

  int ret;
  ret = fallocate(fd, 0, 0, 8192);
  if (ret == -1) {
    printf("fallocate!\n");
    perror(strerror(ret));
  }

  aio_context_t aio_ctx = 0;
  ret = io_setup(64, &aio_ctx);
  if (ret != 0) {
    printf("io_setup error!\n");
    perror(strerror(ret));
  }

  struct iocb iocb;
  memset(&iocb, 0, sizeof(struct iocb));
  static char buf[512] __attribute__ ((__aligned__ (512)));
  strcpy(buf, "bbbbbbbbbb");
  ret = pwrite(fd, buf, 512, 0);
  printf("ret: %d\n", ret);
  perror(strerror(errno));
  iocb.aio_buf = (uintptr_t)buf;
  iocb.aio_nbytes = 512;
  iocb.aio_offset = 0;
  iocb.aio_lio_opcode = IOCB_CMD_PWRITE;
  iocb.aio_fildes = fd;

  struct iocb* ptr = &iocb;

  ret = io_submit(aio_ctx, 1, &ptr);
  if (ret != 1) {
    printf("io_submit error!\n");
    perror(strerror(ret));
  }

  struct io_event* events = malloc(sizeof(struct io_event) * 64);
  ret = io_getevents(aio_ctx, 1, 64, events, NULL);
  if (ret != 1)  {
    printf("io_getevents error!\n");
    perror(strerror(ret));
  }

  printf("return: %lld\n", events[0].res);
  perror(strerror(events[0].res));

  io_destroy(aio_ctx);

  close(fd);
  return 0;
}