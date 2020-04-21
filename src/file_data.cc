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

ssize_t FileData::LocalFile::Read(char* buf, size_t size, size_t off) {
  if (fd < 0)
    Open();
  return pread(fd, buf, size, off);
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

ssize_t FileData::Read(FileHandle* handle, char* buf, size_t size, size_t off) {
  File* file = handle->file;
  if (size + off <= aggr_block_size_) {
    // read aggregation file
    return aggr_files_[file->aggr.file - 1].Read(buf, size, file->aggr.no * aggr_block_size_ + off);
  } else if (off >= aggr_block_size_) {
    // read large file
    auto iter = large_files_.find(file->st_ino);
    if (iter != large_files_.end())
      return iter->second.Read(buf, size, off - aggr_block_size_);
    else
      return 0;
  } else {
    // read both aggregation file and large file
    ssize_t res = aggr_files_[file->aggr.file - 1].Read(buf, aggr_block_size_ - off,
        file->aggr.no * aggr_block_size_ + off);
    assert(res > 0);
    auto iter = large_files_.find(file->st_ino);
    if (iter != large_files_.end())
      res += iter->second.Read(buf + res, size - (aggr_block_size_ - off), 0);
    return res;
  }
}

void FileData::ReadBuf(FileHandle* handle, fuse_bufvec* bufvec,
                       size_t size, size_t off) {
  File* file = handle->file;
  if (size + off <= aggr_block_size_) {
    // read aggregation file
    *bufvec = FUSE_BUFVEC_INIT(size);
    bufvec->buf[0].flags = static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
    bufvec->buf[0].fd = aggr_files_[file->aggr.file - 1].fd;
    bufvec->buf[0].pos = file->aggr.no * aggr_block_size_ + off;
  } else if (off >= aggr_block_size_) {
    // read large file
    *bufvec = FUSE_BUFVEC_INIT(0);
    auto iter = large_files_.find(file->st_ino);
    if (iter != large_files_.end()) {
      bufvec->buf[0].flags = static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
      bufvec->buf[0].fd = iter->second.fd;
      bufvec->buf[0].pos = off - aggr_block_size_;
      bufvec->buf[0].size = size;
    }
  } else {
    auto iter = large_files_.find(file->st_ino);
    if (iter != large_files_.end()) {
      bufvec->count = 2;
      bufvec->idx = 0;
      bufvec->off = 0;
      bufvec->buf[0].flags = static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
      bufvec->buf[0].fd = aggr_files_[file->aggr.file - 1].fd;
      bufvec->buf[0].pos = file->aggr.no * aggr_block_size_ + off;
      bufvec->buf[0].size = aggr_block_size_ - off;
      bufvec->buf[1].flags = static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
      bufvec->buf[1].fd = iter->second.fd;
      bufvec->buf[1].pos = 0;
      bufvec->buf[1].size = size - (aggr_block_size_ - off);
    } else {
      *bufvec = FUSE_BUFVEC_INIT(aggr_block_size_ - off);
      bufvec->buf[0].flags = static_cast<fuse_buf_flags>(FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
      bufvec->buf[0].fd = aggr_files_[file->aggr.file - 1].fd;
      bufvec->buf[0].pos = file->aggr.no * aggr_block_size_ + off;
    }
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
    delete aggr_data;
    delete large_data;
    return res;
  }
}

} // namespace ktablefs