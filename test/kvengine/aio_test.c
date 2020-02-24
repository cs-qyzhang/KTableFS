#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "kvengine/aio-wrapper.h"

int main(void) {
  int fd = open("test.txt", O_CREAT | O_RDWR, 0777);

  fallocate(fd, 0, 0, 4096);

  aio_context_t aio_ctx;
  int ret = io_setup(64, &aio_ctx);
  if (ret != 0) {
    perror(strerror(ret));
  }

  struct iocb** iocbs = malloc(sizeof(struct iocb*) * 64);
  iocbs[0] = malloc(sizeof(struct iocb));
  memset(iocbs[0], 0, sizeof(struct iocb));
  char* buf = malloc(11);
  strcpy(buf, "bbbbbbbbbb");
  iocbs[0]->aio_buf = (uintptr_t)buf;
  iocbs[0]->aio_nbytes = 10;
  iocbs[0]->aio_lio_opcode = IOCB_CMD_PWRITE;
  iocbs[0]->aio_fildes = fd;

  ret = io_submit(aio_ctx, 1, iocbs);
  if (ret != 1) {
    perror(strerror(ret));
  }

  struct io_event* events = malloc(sizeof(struct io_event) * 64);
  ret = io_getevents(aio_ctx, 1, 64, events, NULL);
  if (ret != 1)  {
    perror(strerror(ret));
  }

  printf("return: %lld\n", events[0].res);
  perror(strerror(events[0].res));

  io_destroy(aio_ctx);

  close(fd);
  return 0;
}