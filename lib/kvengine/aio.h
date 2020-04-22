#ifndef KTABLEFS_KVENGINE_AIO_H_
#define KTABLEFS_KVENGINE_AIO_H_

#include <linux/aio_abi.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cstdlib>
#include <vector>
#include "kvengine/batch.h"
#include "kvengine_config.h"

namespace kvengine {

class Slice;
class Worker;

class AIO {
 private:
  iocb* iocb_;
  std::vector<std::function<void(AIO*)> > callbacks_;
  io_event* event_;
  // actual data size and off for read request
  size_t read_size_;
  size_t read_off_;
  Batch* batch_;
  bool submit_;

  friend class Worker;
  friend class LocalFileSet;

 public:
  static const int MAX_EVENT = AIO_MAX_EVENTS;

  explicit AIO(Batch* batch)
    : iocb_(new iocb()), event_(nullptr), batch_(batch)
  { iocb_->aio_data = reinterpret_cast<uintptr_t>(this); }
  ~AIO() { delete iocb_; }

  void AddCallback(std::function<void(AIO*)> callback) { callbacks_.push_back(callback); }
  void SetOpcode(int opcode) { iocb_->aio_lio_opcode = opcode; }
  void SetBuf(char* buf) { iocb_->aio_buf = reinterpret_cast<uintptr_t>(buf); }
  void SetFileIO(int fd, size_t size, size_t off);
  void SetReadSizeAndOff(size_t size, size_t off);
  void SetEvent(io_event* event) { event_ = event; }

  io_event* GetEvent() { return event_; }
  char* GetBuf() { return reinterpret_cast<char*>(iocb_->aio_buf); }
  size_t GetReadOff() { return read_off_; }

  void Submit();
  void FakeSubmit();
  void Finish();
  bool Success();

  static int Setup(aio_context_t *ctxp) {
    return syscall(__NR_io_setup, MAX_EVENT, ctxp);
  }

  static int Destroy(aio_context_t ctx) {
    return syscall(__NR_io_destroy, ctx);
  }

  static int Submit(aio_context_t ctx, long nr, struct iocb **iocbpp) {
    return syscall(__NR_io_submit, ctx, nr, iocbpp);
  }

  static int GetEvents(aio_context_t ctx, long min_nr,
                       struct io_event *events, struct timespec *timeout) {
    return syscall(__NR_io_getevents, ctx, min_nr, MAX_EVENT, events, timeout);
  }
};

} // namespace kvengine

#endif // KTABLEFS_KVENGINE_AIO_H_