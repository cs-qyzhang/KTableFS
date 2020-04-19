#ifndef KTABLEFS_UTIL_HASH_H_
#define KTABLEFS_UTIL_HASH_H_

#include <cstdint>
#include <cstddef>

namespace ktablefs {

class Hash {
 public:
  static uint32_t MurmurHash2(const char* key, size_t len, unsigned int seed = 0);
};

} // namespace ktablefs

#endif // KTABLEFS_UTIL_HASH_H_