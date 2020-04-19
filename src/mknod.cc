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

class MknodCallback {
 private:
  fuse_req_t req_;
  FileHandle* handle_;

 public:
  MknodCallback(fuse_req_t req, FileHandle* handle)
    : req_(req), handle_(handle) {}

  void operator()(Respond* respond) {
    if (respond->res < 0)
      fuse_reply_err(req_, -respond->res);
    else {
      fuse_entry_param e = handle_->FuseEntry();
      fuse_reply_entry(req_, &e);
    }
  }
};

void KTableFS::Mknod(fuse_req_t req, fuse_ino_t parent, const char* name,
                     mode_t mode, dev_t rdev) {
  FileHandle* new_file = new FileHandle(File::REGULAR, mode);
  FileHandle* parent_handle = fs->Handle(req, parent);

  FileKey key(parent_handle->Ino(), new Slice(name, strlen(name), true));
  Batch batch;
  batch.Put(key.ToSlice(), new_file->ToSlice());
  batch.AddCallback(MknodCallback(req, new_file));
  fs->db_->Submit(&batch);
}

} // namespace ktablefs