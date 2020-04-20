#include "kvengine/slice.h"
#include "file_key.h"

namespace ktablefs {

FileKey::FileKey(const Slice* s) {
  char* p = const_cast<char*>(s->data());
  char* ino_buf = reinterpret_cast<char*>(&dir_ino);
  for (int i = sizeof(dir_ino) - 1; i >= 0; --i)
    ino_buf[i] = *p++;
  hash = *reinterpret_cast<uint32_t*>(p);
  p += sizeof(uint32_t);
  name = new Slice(s->data() + (sizeof(ino_t) + sizeof(uint32_t)),
      s->size() - (sizeof(ino_t) + sizeof(uint32_t)), true);
}

Slice* FileKey::ToSlice() {
  size_t name_size = name == nullptr ? 0 : name->size();
  char* buf = new char[sizeof(ino_t) + sizeof(uint32_t) + name_size];
  char* p = buf;
  char* ino_buf = reinterpret_cast<char*>(&dir_ino);
  for (int i = sizeof(dir_ino) - 1; i >= 0; --i)
    *p++ = ino_buf[i];
  *reinterpret_cast<uint32_t*>(p) = hash;
  p += sizeof(uint32_t);
  if (name)
    memcpy(p, name->data(), name_size);
  return new Slice(buf, sizeof(ino_t) + sizeof(uint32_t) + name_size);
}

} // namespace ktablefs