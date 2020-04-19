#ifndef KTABLEFS_KVENGINE_FILESLAB_H_
#define KTABLEFS_KVENGINE_FILESLAB_H_

#include <vector>
#include <list>
#include "kvengine/slice.h"
#include "local_fileset.h"

namespace kvengine {

class AIO;

// Plain Old Data
struct slab_t {
  uint64_t size  : 16;  // slab size
  uint64_t file  : 16;  // file_t used in LocalFileSet
  uint64_t index : 32;  // slab index
};

class FileSlab {
 private:
  struct SingleFileSlab {
    std::vector<file_t> files;
    size_t slab_size;
    unsigned int slab_per_file;
    file_t cur_file;
    unsigned int cur_slab;
    std::list<slab_t> free_slab_;

    SingleFileSlab(unsigned int slab_per_file, size_t slab_size)
      : slab_size(slab_size), slab_per_file(slab_per_file),
        cur_file(0), cur_slab(slab_per_file)
    {}
  };

  LocalFileSet file_set_;
  std::vector<SingleFileSlab> slabs_;

  static const size_t VALID = (1 << 10);

 public:
  explicit FileSlab(std::string directory, int slab_per_file,
                    std::initializer_list<int> slab_size)
    : file_set_(directory)
  {
    for (auto size : slab_size) {
      slabs_.emplace_back(slab_per_file, size);
    }
  }
  FileSlab(FileSlab&&) = delete;
  FileSlab(const FileSlab&) = delete;
  ~FileSlab() {}

  static bool Valid(uint16_t valid) { return valid == VALID; }
  slab_t Create(const Slice* key, const Slice* value);
  void Read(slab_t slab, AIO* aio);
  void Write(slab_t slab, const Slice* key, const Slice* value, AIO* aio);
  void Delete(slab_t slab, AIO* aio);
};

} // namespace kvengine

#endif // KTABLEFS_KVENGINE_FILESLAB_H_