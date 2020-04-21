#include <unistd.h>
#include <fcntl.h>
#include <cassert>
#include <cerrno>
#include <iostream>
#include "kvengine/slice.h"
#include "file.h"
#include "file_data.h"

namespace ktablefs {

using kvengine::Slice;

void FileData::LocalFile::Open() {
  if (fd <= 0) {
    fd = open(file_name.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd <= 0)
      perror(strerror(errno));
  }
}

void FileData::LocalFile::Close() {
  if (fd > 0)
    close(fd);
}

Slice* FileData::LocalFile::Read(size_t size, size_t off) {
  if (fd < 0)
    Open();
  char* buf = new char[size];
  ssize_t res = pread(fd, buf, size, off);
  assert(res >= 0);
  return new Slice(buf, res);
}

void FileData::LocalFile::Delete() {
  Close();
  remove(file_name.c_str());
}

ssize_t FileData::LocalFile::Write(const Slice* data, size_t off) {
  if (fd < 0)
    Open();
  ssize_t res = pwrite(fd, data->data(), data->size(), off);
  assert(res == static_cast<ssize_t>(data->size()));
  return res;
}

aggr_t FileData::Create() {
  aggr_t res;
  if (!aggr_free_.empty()) {
    res = aggr_free_.front();
    aggr_free_.pop_front();
  } else {
    if (cur_aggr_index_ == 0x10000) {
      cur_aggr_file_++;
      cur_aggr_index_ = 0;
      aggr_files_.emplace_back(work_dir_ + "/aggr_" + std::to_string(cur_aggr_file_));
    } else {
      cur_aggr_index_++;
    }
    res.file = cur_aggr_file_;
    res.no = cur_aggr_index_;
  }
  return res;
}

void FileData::Delete(FileHandle* handle) {
  File* file = handle->file;
  aggr_free_.emplace_back(file->aggr);
  auto iter = large_files_.find(file->st_ino);
  if (iter != large_files_.end()) {
    iter->second.Delete();
    large_files_.erase(iter);
  }
}

Slice* FileData::Read(FileHandle* handle, size_t size, size_t off) {
  File* file = handle->file;
  if (size + off <= aggr_block_size_) {
    return aggr_files_[file->aggr.file - 1].Read(size, file->aggr.no * aggr_block_size_ + off);
  } else if (off >= aggr_block_size_) {
    auto iter = large_files_.find(file->st_ino);
    if (iter != large_files_.end()) {
      return iter->second.Read(size, off - aggr_block_size_);
    } else {
      return new Slice(nullptr, 0);
    }
  } else {
    Slice* aggr_data = aggr_files_[file->aggr.file - 1].Read(aggr_block_size_ - off,
        file->aggr.no * aggr_block_size_ + off);
    auto iter = large_files_.find(file->st_ino);
    Slice* large_data;
    if (iter != large_files_.end())
      large_data = iter->second.Read(size - (aggr_block_size_ - off), 0);
    else
      large_data = new Slice(nullptr, 0);
    Slice* res;
    char* buf = new char[aggr_data->size() + large_data->size()];
    memcpy(buf, aggr_data->data(), aggr_data->size());
    memcpy(buf + aggr_data->size(), large_data->data(), large_data->size());
    res = new Slice(buf, aggr_data->size() + large_data->size());
    return res;
  }
}

// Update File Size
ssize_t FileData::Write(FileHandle* handle, const Slice* data, size_t off) {
  File* file = handle->file;
  size_t size = data->size();
  if (size + off <= aggr_block_size_) {
    return aggr_files_[file->aggr.file - 1].Write(data, file->aggr.no * aggr_block_size_ + off);
  } else if (off >= aggr_block_size_) {
    auto iter = large_files_.find(file->st_ino);
    if (iter != large_files_.end()) {
      return iter->second.Write(data, off - aggr_block_size_);
    } else {
      auto new_file = large_files_.emplace(file->st_ino,
          work_dir_ + "/large_" + std::to_string(file->st_ino));
      assert(new_file.second);
      return new_file.first->second.Write(data, off - aggr_block_size_);
    }
  } else {
    ssize_t res = 0;
    Slice* aggr_data = new Slice(data->data(), aggr_block_size_ - off);
    Slice* large_data = new Slice(data->data() + (aggr_block_size_ - off),
        size - (aggr_block_size_ - off));
    res += aggr_files_[file->aggr.file - 1].Write(aggr_data, file->aggr.no * aggr_block_size_ + off);

    auto iter = large_files_.find(file->st_ino);
    if (iter != large_files_.end()) {
      res += iter->second.Write(large_data, 0);
    } else {
      auto new_file = large_files_.emplace(file->st_ino,
          work_dir_ + "/large_" + std::to_string(file->st_ino));
      assert(new_file.second);
      res += new_file.first->second.Write(large_data, 0);
    }
    assert(res == static_cast<ssize_t>(size));
    return res;
  }
}

} // namespace ktablefs