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
};

} // namespace ktablefs

#endif // KTABLEFS_FILE_KEY_H_