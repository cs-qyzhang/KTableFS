#include "file.h"
#include "ktablefs.h"

namespace ktablefs {

FileHandle::FileHandle() {
  memset(&file_, 0, sizeof(file_));
}

FileHandle::FileHandle(const kvengine::Slice* s) {
  char* p = const_cast<char*>(s->data());
  file_.type = *reinterpret_cast<uint8_t*>(p);
  if (IsSymlink() && IsRegular()) {

  } else {
    assert(s->size() == sizeof(file_));
    memcpy(&file_, p, sizeof(file_));
  }
}

FileHandle::FileHandle(uint8_t type, mode_t mode) {
  memset(&file_, 0, sizeof(file_));
  file_.st_nlink = 1;
  file_.st_mode = mode;
  file_.st_ino = KTableFS::fs->NewIno();
  file_.type = type;
  file_.aggr = KTableFS::fs->file_data_->Create();
  UpdateATime();
  UpdateMTime();
}

Slice* FileHandle::ToSlice() {
  if (IsSymlink() && IsRegular()) {
    // TODO
    return nullptr;
  } else {
    char* buf = new char[sizeof(file_)];
    memcpy(buf, &file_, sizeof(file_));
    return new Slice(buf, sizeof(file_));
  }
}

fuse_entry_param FileHandle::FuseEntry() {
  fuse_entry_param e;
  memset(&e, 0, sizeof(e));
  e.attr.st_atime   = file_.atime;
  e.attr.st_mtime   = file_.mtime;
  e.attr.st_ctime   = file_.mtime;
  e.attr.st_ino     = file_.st_ino;
  e.attr.st_mode    = file_.st_mode;
  e.attr.st_nlink   = file_.st_nlink;
  e.attr.st_size    = file_.st_size;
  e.attr.st_uid     = KTableFS::fs->st_uid;
  e.attr.st_gid     = KTableFS::fs->st_gid;
  e.attr.st_blksize = KTableFS::fs->st_blksize;
  e.attr.st_dev     = KTableFS::fs->st_dev;
  e.attr.st_rdev    = KTableFS::fs->st_rdev;
  e.attr_timeout    = KTableFS::fs->timeout;
  e.entry_timeout   = KTableFS::fs->timeout;
  e.ino             = reinterpret_cast<fuse_ino_t>(this);
  return e;
}

} // namespace ktablefs