#include "kvengine/slice.h"
#include "file_key.h"

namespace ktablefs {

FileKey::FileKey(const Slice* s) {
  char* p = const_cast<char*>(s->data());
  dir_ino = *reinterpret_cast<ino_t*>(p);
  p += sizeof(ino_t);
  hash = *reinterpret_cast<uint32_t*>(p);
  p += sizeof(uint32_t);
  name = new Slice(s->data() + (sizeof(ino_t) + sizeof(uint32_t)),
      s->size() - (sizeof(ino_t) + sizeof(uint32_t)), true);
}

Slice* FileKey::ToSlice() {
  char* buf = new char[sizeof(ino_t) + sizeof(uint32_t) + name->size()];
  char* p = buf;
  *reinterpret_cast<ino_t*>(p) = dir_ino;
  p += sizeof(ino_t);
  *reinterpret_cast<uint32_t*>(p) = hash;
  p += sizeof(uint32_t);
  memcpy(p, name->data(), name->size());
  return new Slice(buf, sizeof(ino_t) + sizeof(uint32_t) + name->size());
}


} // namespace ktablefs