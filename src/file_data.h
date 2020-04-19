#ifndef KTABLEFS_AGGREGATE_FILE_H_
#define KTABLEFS_AGGREGATE_FILE_H_

#include <cstdint>
#include <vector>
#include <map>
#include <bitset>
#include <list>
#include "kvengine/slice.h"

namespace ktablefs {

using kvengine::Slice;
class FileHandle;

struct aggr_t {
  uint16_t file;
  uint16_t no;
};

class FileData {
 private:
  struct LocalFile {
    int fd;
    std::string file_name;

    explicit LocalFile(std::string file_name)
      : fd(0), file_name(file_name)
    { Open(); }

    void Open();
    void Close();
    void Delete();
    Slice* Read(size_t size, size_t off);
    ssize_t Write(const Slice* data, size_t off);
  };

  std::string work_dir_;
  std::vector<LocalFile> aggr_files_;
  std::map<ino_t, LocalFile> large_files_;
  size_t aggr_block_size_;
  int cur_aggr_file_;
  int cur_aggr_index_;
  std::list<aggr_t> aggr_free_;

 public:
  FileData(std::string work_dir)
    : work_dir_(work_dir), aggr_block_size_(8192), cur_aggr_file_(0), cur_aggr_index_(0x10000) {}
  aggr_t Create();
  void Delete(FileHandle* handle);
  Slice* Read(FileHandle* handle, size_t size, size_t off);
  ssize_t Write(FileHandle* handle, const Slice* data, size_t off);
};

} // namespace ktablefs

#endif // KTABLEFS_AGGREGATE_FILE_H_