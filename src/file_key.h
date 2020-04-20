#ifndef KTABLEFS_FILE_KEY_H_
#define KTABLEFS_FILE_KEY_H_

#include <cstdint>
#include "kvengine/slice.h"

namespace ktablefs {

using kvengine::Slice;

struct FileKey {
  ino_t dir_ino;
  uint32_t hash;
  Slice* name;

  FileKey() : dir_ino(0), hash(0), name(nullptr) {}
  FileKey(ino_t ino, Slice* n) : dir_ino(ino), hash(n->hash()), name(n) {}
  FileKey(const Slice*);

  Slice* ToSlice();

  bool operator<(const FileKey& b) const {
    if (dir_ino < b.dir_ino) {
      return true;
    } else if (dir_ino == b.dir_ino) {
      if (hash < b.hash)
        return true;
      else if (hash == b.hash)
        return *name < *b.name;
      else
        return false;
    } else {
      return false;
    }
  }

  bool operator==(const FileKey& b) const {
    return (dir_ino == b.dir_ino && hash == b.hash && *name == *b.name);
  }

  bool operator!=(const FileKey& b) const {
    return !operator==(b);
  }

};

} // namespace ktablefs

#endif // KTABLEFS_FILE_KEY_H_