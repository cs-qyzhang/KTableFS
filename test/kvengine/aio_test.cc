#include <fcntl.h>
#include <cstring>
#include <cassert>
#include <iostream>
#include "aio-wrapper.h"

using namespace std;

int main(void) {
  int fd = open("./test", O_CREAT | O_RDWR | O_TRUNC, 0644);
  char* wbuf = new char[1024];
  sprintf(wbuf, "hello, world!");
  char* wbuf2 = new char[1024];
  sprintf(wbuf2, "hello, world!");
  char* rbuf = new char[1024];
  char* rbuf2 = new char[1024];
  iocb** iocbs = new iocb*[64];

  iocbs[0] = new iocb;
  iocbs[0]->aio_lio_opcode = IOCB_CMD_PWRITE;
  iocbs[0]->aio_buf = reinterpret_cast<uintptr_t>(wbuf);
  iocbs[0]->aio_fildes = fd;
  iocbs[0]->aio_nbytes = 20;
  iocbs[0]->aio_offset = 0;

  iocbs[1] = new iocb;
  iocbs[1]->aio_lio_opcode = IOCB_CMD_PREAD;
  iocbs[1]->aio_buf = reinterpret_cast<uintptr_t>(rbuf);
  iocbs[1]->aio_fildes = fd;
  iocbs[1]->aio_nbytes = 20;
  iocbs[1]->aio_offset = 0;

  iocbs[2] = new iocb;
  iocbs[2]->aio_lio_opcode = IOCB_CMD_PWRITE;
  iocbs[2]->aio_buf = reinterpret_cast<uintptr_t>(wbuf2);
  iocbs[2]->aio_fildes = fd;
  iocbs[2]->aio_nbytes = 20;
  iocbs[2]->aio_offset = 3;

  iocbs[3] = new iocb;
  iocbs[3]->aio_lio_opcode = IOCB_CMD_PREAD;
  iocbs[3]->aio_buf = reinterpret_cast<uintptr_t>(rbuf2);
  iocbs[3]->aio_fildes = fd;
  iocbs[3]->aio_nbytes = 20;
  iocbs[3]->aio_offset = 0;

  aio_context_t ctx = 0;
  int res = io_setup(32, &ctx);
  perror(strerror(errno));
  assert(res == 0);
  res = io_submit(ctx, 4, iocbs);
  assert(res == 4);

  io_event* events = new io_event[32];
  res = io_getevents(ctx, 4, 32, events, nullptr);
  perror(strerror(errno));
  assert(res == 4);

  cout << events[0].res << endl;
  cout << events[1].res << endl;
  cout << events[2].res << endl;
  cout << events[4].res << endl;
  cout << rbuf << endl;
  cout << rbuf2 << endl;

  io_destroy(ctx);

  return 0;
}