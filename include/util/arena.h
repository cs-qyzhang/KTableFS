#ifndef KTABLEFS_UTIL_ARENA_H_
#define KTABLEFS_UTIL_ARENA_H_

#include <sys/types.h>


namespace ktablefs {

constexpr size_t PAGE_SIZE = 4096;

class Arena {
 private:
  std::vector<char*> blocks_;
  size_t block_size_;
  size_t remain_;

 public:
  Arena()
    :block_size_{PAGE_SIZE}, remain_{0} {}
  Arena(size_t block_size)
    :block_size_{block_size}, remain_{0} {}
  ~Arena() { for (auto block : blocks_) delete block; }

  Allocate(size_t size);
  AlignedAllocate(size_t align, size_t size);
};

} // namespace ktablefs

#endif // KTABLEFS_UTIL_ARENA_H_