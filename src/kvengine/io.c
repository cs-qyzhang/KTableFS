#include <stdlib.h>
#include <string.h>
#include "kvengine/aio-wrapper.h"
#include "kvengine/io.h"
#include "kvengine/hash.h"
#include "ktablefs_config.h"

extern struct queue* io_que;

void io_worker_send_request(struct iocb* iocb, int fd, int page_index) {
  
}

int io_write_page(hash_t hash, char* page) {
  int fd;
  int page_index;
  get_page_from_hash(hash, &fd, &page_index);
}

int io_read_page(hash_t hash, char* page) {
  int fd;
  int page_index;
  get_page_from_hash(hash, &fd, &page_index);
  struct iocb* iocb = malloc(sizeof(*iocb));
  memset(iocb, 0, sizeof(*iocb));
  iocb->aio_buf = page;
  iocb->aio_fildes = fd;
  iocb->aio_offset = PAGE_SIZE * page_index;
  iocb->aio_nbytes = PAGE_SIZE;
  iocb->aio_lio_opcode = IOCB_CMD_PREAD;

  io_worker_send_request(iocb, fd, page_index);
}