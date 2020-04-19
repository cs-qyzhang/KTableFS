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
  Slice* res = fs->file_data_->Read(handle, size, off);
  if (res) {
    int err = fuse_reply_buf(req, res->data(), res->size());
    assert(err == 0);
    delete res;
  } else {
    assert(0);
  }
}

void KTableFS::Write(fuse_req_t req, fuse_ino_t ino, const char *buf,
		                 size_t size, off_t off, struct fuse_file_info *fi) {
  FileHandle* handle = fs->Handle(req, ino);
  ssize_t res = fs->file_data_->Write(handle, new Slice(buf, size), off);
  if (res > 0)
    fuse_reply_write(req, res);
  else
    fuse_reply_err(req, -res);
}

} // namespace ktablefs