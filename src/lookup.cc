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

class LookupCallback {
 private:
  fuse_req_t req_;

 public:
  LookupCallback(fuse_req_t req) : req_(req) {}
  void operator()(Respond* respond) {
    if (respond->res < 0)
      fuse_reply_err(req_, -respond->res);
    else {
      FileHandle* handle = new FileHandle(respond->data);
      if (handle->IsRegular()) {
        fuse_entry_param e = handle->FuseEntry();
        fuse_reply_entry(req_, &e);
      } else {

      }
    }
  }
};

void KTableFS::Lookup(fuse_req_t req, fuse_ino_t parent, const char* name) {
  FileHandle* handle = fs->Handle(req, parent);

  FileKey key(handle->Ino(), new Slice(name, strlen(name), true));
  Batch batch;
  batch.Get(key.ToSlice());
  batch.AddCallback(LookupCallback(req));
  fs->db_->Submit(&batch);
}

} // namespace ktablefs