#ifndef KTABLEFS_KVENGINE_LOCAL_FILESET_H_
#define KTABLEFS_KVENGINE_LOCAL_FILESET_H_

#include <vector>
#include "pagecache.h"

namespace kvengine {

class AIO;
class Slice;

typedef unsigned int file_t;

// Manage local file
class LocalFileSet {
 private:
  struct LocalFile {
    int fd;
    mode_t mode;
    std::string file_name;

    explicit LocalFile(std::string file_name, mode_t mode);

    void Open();
    void Close();
  };

  std::vector<LocalFile> file_set_;
  std::string directory_;
  PageCache pagecache_;

  uint64_t PageHash_(file_t file, int page_index) {
    return ((uint64_t)file << 40U) + (uint64_t)page_index;
  }

 public:
  explicit LocalFileSet(std::string directory);
  LocalFileSet(const LocalFileSet&) = delete;
  LocalFileSet(LocalFileSet&&) = delete;
  ~LocalFileSet() {}

  // @file_name relative to LocalFileSet::directory_
  file_t Create(std::string file_name, mode_t mode);
  // @file_name relative to LocalFileSet::directory_
  file_t Open(std::string file_name);
  void Close(file_t file);
  Slice* ReadSync(file_t file, size_t size, off_t off);
  void Read(file_t file, size_t size, off_t off, AIO* aio);
  void Write(file_t file, const Slice* data, off_t off, AIO* aio);
};

} // namespace kvengine

#endif // KTABLEFS_KVENGINE_LOCAL_FILESET_H_