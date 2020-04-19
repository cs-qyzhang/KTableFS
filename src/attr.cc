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

void KTableFS::GetAttr(fuse_req_t req, fuse_ino_t ino,
			                 struct fuse_file_info *fi) {
  FileHandle* handle = fs->Handle(req, ino);
  fuse_entry_param e = handle->FuseEntry();
  fuse_reply_entry(req, &e);
}

} // namespace ktablefs