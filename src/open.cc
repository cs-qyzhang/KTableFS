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

void KTableFS::Open(fuse_req_t req, fuse_ino_t ino,
		                struct fuse_file_info *fi) {
  FileHandle* handle = fs->Handle(req, ino);
  fi->fh = ino;
  fuse_reply_open(req, fi);
  KTableFS::IndexInsert(*handle->key, handle->file);
}

} // namespace ktablefs