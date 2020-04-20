#include "file.h"
#include "ktablefs.h"

namespace ktablefs {

FileHandle::FileHandle() {
  file = new File();
  memset(file, 0, sizeof(*file));
}

FileHandle::FileHandle(const kvengine::Slice* s, FileKey* file_key)
  : key(file_key)
{
  char* p = const_cast<char*>(s->data());
  file = new File();
  file->type = *reinterpret_cast<uint8_t*>(p);
  if (IsSymlink() && IsRegular()) {

  } else {
    assert(s->size() == sizeof(*file));
    memcpy(file, p, sizeof(*file));
  }
}

FileHandle::FileHandle(uint8_t type, mode_t mode)
  : key(nullptr)
{
  file = new File();
  memset(file, 0, sizeof(*file));
  file->st_mode = mode;
  file->st_ino = KTableFS::fs->NewIno();
  file->type = type;
  if (mode & S_IFDIR) {
    file->aggr = {0};
    // Why "two" hardlinks instead of "one"? The answer is here:
    // http://unix.stackexchange.com/a/101536
    file->st_nlink = 2;
  } else {
    file->aggr = KTableFS::fs->file_data_->Create();
    file->st_nlink = 1;
  }
  UpdateATime();
  UpdateMTime();
}

Slice* FileHandle::ToSlice() const {
  if (IsSymlink() && IsRegular()) {
    // TODO
    return nullptr;
  } else {
    char* buf = new char[sizeof(*file)];
    memcpy(buf, file, sizeof(*file));
    return new Slice(buf, sizeof(*file));
  }
}

fuse_entry_param FileHandle::FuseEntry() const {
  fuse_entry_param e;
  memset(&e, 0, sizeof(e));
  e.attr.st_atime   = file->atime;
  e.attr.st_mtime   = file->mtime;
  e.attr.st_ctime   = file->mtime;
  e.attr.st_ino     = file->st_ino;
  e.attr.st_mode    = file->st_mode;
  e.attr.st_nlink   = file->st_nlink;
  e.attr.st_size    = file->st_size;
  e.attr.st_uid     = KTableFS::fs->st_uid;
  e.attr.st_gid     = KTableFS::fs->st_gid;
  e.attr.st_blksize = KTableFS::fs->st_blksize;
  e.attr.st_blocks  = (file->st_size + e.attr.st_blksize - 1) / e.attr.st_blksize;
  e.attr.st_dev     = KTableFS::fs->st_dev;
  e.attr.st_rdev    = KTableFS::fs->st_rdev;
  e.attr_timeout    = KTableFS::fs->timeout;
  e.entry_timeout   = KTableFS::fs->timeout;
  e.ino             = reinterpret_cast<fuse_ino_t>(this);
  return e;
}

} // namespace ktablefs