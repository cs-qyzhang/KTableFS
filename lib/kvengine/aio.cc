#include "kvengine/batch.h"
#include "worker.h"
#include "aio.h"

namespace kvengine {

void AIO::SetFileIO(int fd, size_t size, size_t off) {
  iocb_->aio_fildes = fd;
  iocb_->aio_nbytes = size;
  iocb_->aio_offset = off;
}

void AIO::SetReadSizeAndOff(size_t size, size_t off) {
  read_size_ = size;
  read_off_ = off;
}

void AIO::Submit() {
  submit_ = true;
  batch_->cur_worker_->IOEnqueue(this);
}

void AIO::FakeSubmit() {
  submit_ = false;
  batch_->cur_worker_->IOEnqueue(this);
}

bool AIO::Success() {
  if (iocb_->aio_lio_opcode == IOCB_CMD_PREAD) {
    return (event_ == nullptr || event_->res > 0);
  } else if (iocb_->aio_lio_opcode == IOCB_CMD_PWRITE) {
    return (event_ && event_->res == static_cast<ssize_t>(iocb_->aio_nbytes));
  } else {
    assert(0);
    return false;
  }
}

void AIO::Finish() {
  for (auto callback : callbacks_)
    callback(this);
  if (iocb_->aio_lio_opcode == IOCB_CMD_PREAD) {
    char* read_buf = reinterpret_cast<char*>(iocb_->aio_buf);
    Slice* data = new Slice(read_buf + read_off_, read_size_, true);
    batch_->FinishRequest(data, Success() ? 0 : event_->res);
  } else if (iocb_->aio_lio_opcode == IOCB_CMD_PWRITE) {
    batch_->FinishRequest(nullptr, Success() ? 0 : event_->res);
  } else {
    throw "ERROR in AIO::Finish(): unknown iocb opcode!";
  }
}

} // namespace kvengine