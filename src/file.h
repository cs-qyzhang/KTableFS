#ifndef KTABLEFS_FILE_H_
#define KTABLEFS_FILE_H_

#include <sys/types.h>
#include <cstdint>
#include "fuse_header.h"
#include "kvengine/slice.h"
#include "file_data.h"

namespace ktablefs {

class KTableFS;
class FileKey;

// Plain Old Data, 32 byte
struct File {
  static const int REGULAR  = 1;
  static const int HARDLINK = 2;
  static const int SYMLINK  = 4;

  union {
    struct {
      uint32_t type     : 4;
      uint32_t st_nlink : 8;
      uint32_t st_mode  : 20;
      aggr_t aggr;
      uint32_t atime;
      uint32_t mtime; // remove ctime (simply make ctime == atime)
      uint64_t st_ino;
      uint64_t st_size;
    };
    struct {
      uint8_t _;
      uint16_t blob_size;
      char blob[1];
    };
  };
};

static_assert(sizeof(File) == 32);

class FileHandle {
 public:
  File* file;
  FileKey* key;

  FileHandle();
  FileHandle(File* f, FileKey* k) : file(f), key(k) {}
  FileHandle(const kvengine::Slice*, FileKey* key);
  FileHandle(uint8_t, mode_t);

  fuse_entry_param FuseEntry() const;
  Slice* ToSlice() const;

  void SetKey(FileKey* key) { key = key; }
  void SetFile(File* file) { file = file; }

  void UpdateMTime() { file->mtime = time(NULL); }
  void UpdateATime() { file->atime = time(NULL); }
  ino_t Ino() const { return file->st_ino; }

  bool IsRegular() const { return (file->type & File::REGULAR); }
  bool IsSymlink() const { return (file->type & File::SYMLINK); }
  bool IsHardlink() const { return (file->type & File::HARDLINK); }
};

} // namespace ktablefs

#endif // KTABLEFS_FILE_H_