#include "fuse_header.h"
#include "kvengine/slice.h"
#include "kvengine/batch.h"
#include "kvengine/db.h"
#include "ktablefs.h"
#include "file.h"
#include "file_data.h"
#include "file_key.h"

namespace ktablefs {

using kvengine::Slice;
using kvengine::Batch;
using kvengine::DB;
using kvengine::Respond;

void KTableFS::Read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                    struct fuse_file_info *fi) {
  FileHandle* handle = fs->Handle(req, ino);
  char buf[size];
  ssize_t res = fs->file_data_->Read(handle, buf, size, off);
  if (res >= 0) {
    int err = fuse_reply_buf(req, buf, res);
    assert(err == 0);
  } else {
    assert(0);
  }
}

void KTableFS::ReadBuf(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                    struct fuse_file_info *fi) {
  FileHandle* handle = fs->Handle(req, ino);
  char buf[sizeof(fuse_bufvec) + sizeof(fuse_buf)];
  fuse_bufvec* bufvec = reinterpret_cast<fuse_bufvec*>(buf);
  fs->file_data_->ReadBuf(handle, bufvec, size, off);
  fuse_reply_data(req, bufvec, FUSE_BUF_SPLICE_MOVE);
}

class WriteCallback {
 private:
  fuse_req_t req_;
  size_t count_;

 public:
  WriteCallback(fuse_req_t req, size_t count) : req_(req), count_(count) {}
  void operator()(Respond* respond) {
    if (respond->res == 0) {
      fuse_reply_write(req_, count_);
    } else {
      assert(0);
    }
  }
};

void KTableFS::Write(fuse_req_t req, fuse_ino_t ino, const char *buf,
		                 size_t size, off_t off, struct fuse_file_info *fi) {
  FileHandle* handle = fs->Handle(req, ino);
  Slice* data = new Slice(buf, size);
  ssize_t res = fs->file_data_->Write(handle, data, off);
  delete data;
  if (res > 0) {
    if (size + off > handle->file->st_size) {
      // Update File Size
      Batch batch;
      handle->file->st_size = size + off;
      handle->UpdateMTime();
      batch.Update(handle->key->ToSlice(), handle->ToSlice());
      batch.AddCallback(WriteCallback(req, res));
      fs->db_->Submit(&batch);
    } else {
      fuse_reply_write(req, res);
    }
  } else {
    fuse_reply_err(req, -res);
  }
}

} // namespace ktablefs