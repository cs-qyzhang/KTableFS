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

class MkdirCallback {
 private:
  fuse_req_t req_;
  FileHandle* handle_;

 public:
  MkdirCallback(fuse_req_t req, FileHandle* handle)
    : req_(req), handle_(handle) {}

  void operator()(Respond* respond) {
    if (respond->res < 0)
      fuse_reply_err(req_, -respond->res);
    else {
      fuse_entry_param e = handle_->FuseEntry();
      fuse_reply_entry(req_, &e);
      KTableFS::IndexInsert(*handle_->key, handle_->file);
    }
  }
};

void KTableFS::Mkdir(fuse_req_t req, fuse_ino_t parent, const char *name,
		                 mode_t mode) {
  FileHandle* new_file = new FileHandle(File::REGULAR, mode | S_IFDIR);
  FileHandle* parent_handle = fs->Handle(req, parent);

  new_file->key = new FileKey(parent_handle->Ino(), new Slice(name, strlen(name), true));
  Batch batch;
  batch.Put(new_file->key->ToSlice(), new_file->ToSlice());
  batch.AddCallback(MkdirCallback(req, new_file));
  fs->db_->Submit(&batch);
}

void KTableFS::OpenDir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi) {
  FileHandle* handle = fs->Handle(req, ino);
  fi->fh = ino;
  fuse_reply_open(req, fi);
  KTableFS::IndexInsert(*handle->key, handle->file);
}

class ReadDirScan {
 private:
  fuse_req_t req;
  char* buf;
  size_t total_size;
  size_t* buf_size;
  off_t off;
  off_t* cur_off;
  bool plus;

 public:
  ReadDirScan(fuse_req_t req, char* buf, size_t total_size,
              size_t* buf_size, off_t off, off_t* cur_off, bool plus)
    : req(req), buf(buf), total_size(total_size), buf_size(buf_size),
      off(off), cur_off(cur_off), plus(plus) {}

  bool operator()(Slice* key, Slice* value) {
    *cur_off += 1;
    if (*cur_off <= off)
      return true;
    size_t entsize;
    FileKey* file_key = new FileKey(key);
    FileHandle handle(value, file_key);
    fuse_entry_param e = handle.FuseEntry();
    size_t remain = total_size - *buf_size;
    if (plus) {
      entsize = fuse_add_direntry_plus(req, buf + *buf_size, remain,
                    file_key->name->ToString().c_str(), &e, *cur_off);
    } else {
      entsize = fuse_add_direntry(req, buf + *buf_size, remain,
                    file_key->name->ToString().c_str(), &e.attr, *cur_off);
    }
    // buf is small, operation failed.
    if (entsize > remain)
      return false;
    else
      *buf_size += entsize;
    return true;
  }
};

class ReadDirCallback {
 private:
  fuse_req_t req;
  char* buf;
  size_t* buf_size;

 public:
  ReadDirCallback(fuse_req_t req, char* buf, size_t* buf_size)
    : req(req), buf(buf), buf_size(buf_size) {}

  void operator()(Respond* respond) {
    fuse_reply_buf(req, buf, *buf_size);
    delete buf_size;
  }
};

void KTableFS::ReadDir_(fuse_req_t req, fuse_ino_t ino, size_t size,
                        off_t off, struct fuse_file_info *fi, bool plus) {
  FileHandle* handle = fs->Handle(req, ino);
  FileKey min_key;
  min_key.dir_ino = handle->file->st_ino;
  FileKey max_key;
  max_key.dir_ino = handle->file->st_ino + 1;

  char* buf = new char[size];
  off_t* cur_off = new off_t;
  *cur_off = 0;
  size_t* buf_size = new size_t;
  *buf_size = 0;

  Batch batch;
  batch.Scan(min_key.ToSlice(), max_key.ToSlice(), ReadDirScan(req, buf, size, buf_size, off, cur_off, plus));
  batch.AddCallback(ReadDirCallback(req, buf, buf_size));
  fs->db_->Submit(&batch);
}

void KTableFS::ReadDir(fuse_req_t req, fuse_ino_t ino, size_t size,
                       off_t off, struct fuse_file_info *fi) {
  ReadDir_(req, ino, size, off, fi, false);
}

void KTableFS::ReadDirPlus(fuse_req_t req, fuse_ino_t ino, size_t size,
                           off_t off, struct fuse_file_info *fi) {
  ReadDir_(req, ino, size, off, fi, true);
}

} // namespace ktablefs