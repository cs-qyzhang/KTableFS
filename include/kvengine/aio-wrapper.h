#ifndef KTABLEFS_KVENGINE_AIO_WAPPER_H_
#define KTABLEFS_KVENGINE_AIO_WAPPER_H_

#include <linux/aio_abi.h>
#include <sys/syscall.h>

static inline int io_setup(unsigned max_nr, aio_context_t *ctxp) {
  return syscall(__NR_io_setup, max_nr, ctxp);
}

static inline int io_destroy(aio_context_t ctx) {
  return syscall(__NR_io_destroy, ctx);
}

static inline int io_submit(aio_context_t ctx, long nr, struct iocb **iocbpp) {
  return syscall(__NR_io_submit, ctx, nr, iocbpp);
}

static inline int io_getevents(aio_context_t ctx, long min_nr, long max_nr,
                               struct io_event *events,
                               struct timespec *timeout) {
  return syscall(__NR_io_getevents, ctx, min_nr, max_nr, events, timeout);
}

#endif // KTABLEFS_KVENGINE_AIO_WAPPER_H_