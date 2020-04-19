#ifndef KTABLEFS_FILE_H_
#define KTABLEFS_FILE_H_

#include <sys/types.h>
#include <cstdint>
#include "fuse_header.h"
#include "kvengine/slice.h"
#include "file_data.h"

namespace ktablefs {

class KTableFS;

// Plain Old Data, 32 byte
struct File {
  static const int REGULAR  = 1;
  static const int HARDLINK = 2;
  static const int SYMLINK  = 4;

  union {
    struct {
      uint8_t type;
      uint8_t st_nlink;
      uint16_t st_mode;
      aggr_t aggr;
      uint32_t atime;
      uint32_t mtime; // remove ctime (simply make ctime == atime)
      ino_t st_ino;
      off_t st_size;
    };
    struct {
      uint8_t _;
      uint16_t blob_size;
      char blob[1];
    };
  };
};

class FileHandle {
 private:
  File file_;

  friend class KTableFS;
  friend class FileData;

 public:
  FileHandle();
  FileHandle(const kvengine::Slice*);
  FileHandle(uint8_t, mode_t);
  fuse_entry_param FuseEntry();
  Slice* ToSlice();

  void UpdateMTime() { file_.mtime = time(NULL); }
  void UpdateATime() { file_.atime = time(NULL); }
  ino_t Ino() { return file_.st_ino; }

  bool IsRegular() { return (file_.type & File::REGULAR); }
  bool IsSymlink() { return (file_.type & File::SYMLINK); }
  bool IsHardlink() { return (file_.type & File::HARDLINK); }
};

} // namespace ktablefs

#endif // KTABLEFS_FILE_H_