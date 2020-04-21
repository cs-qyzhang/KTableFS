#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <iostream>
#include "kvengine/slice.h"
#include "pagecache.h"
#include "local_fileset.h"
#include "aio.h"

namespace kvengine {

LocalFileSet::LocalFile::LocalFile(std::string file_name, mode_t mode)
  : fd(0), mode(mode), file_name(file_name)
{
  fd = open(file_name.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd <= 0) {
    std::cerr << "LocalFile error while creating file " << file_name << ":";
    std::cerr << std::strerror(errno) << std::endl;
  }
}

void LocalFileSet::LocalFile::Open() {
  if (fd <= 0) {
    fd = open(file_name.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd <= 0) {
      std::cerr << "LocalFile error while opening file " << file_name << ":";
      std::cerr << std::strerror(errno) << std::endl;
    }
  }
}

void LocalFileSet::LocalFile::Close() {
  if (fd > 0)
    close(fd);
  fd = 0;
}

LocalFileSet::LocalFileSet(std::string directory)
  : directory_(directory), pagecache_(1024*128)
{
  // valid directory, remove last '/'
  // TODO: stat()..
  assert(directory.size() > 1);
  if (directory_[directory_.size() - 1] == '/') {
    directory_.pop_back();
  }
}

file_t LocalFileSet::Create(std::string file_name, mode_t mode) {
  file_t new_file = file_set_.size() + 1;
  assert(new_file > 0);
  file_set_.emplace_back(directory_ + file_name, mode);
  return new_file;
}

file_t LocalFileSet::Open(std::string file_name) {
  for (file_t i = 0; i < file_set_.size(); ++i) {
    if (file_set_[i].file_name == file_name) {
      file_set_[i].Open();
      return i + 1;
    }
  }
  return 0;
}

void LocalFileSet::Close(file_t file) {
  file_set_[file - 1].Close();
}

Slice* LocalFileSet::ReadSync(file_t file, size_t size, off_t off) {
  file_set_[file - 1].Open();
  int page_index = off / PageCache::PAGE_SIZE;
  PageCache::Entry* ent = pagecache_.Lookup(PageHash_(file, page_index));
  if (ent == nullptr) {
    ent = pagecache_.NewEntry(PageHash_(file, page_index));
    ssize_t cnt = pread(file_set_[file - 1].fd, ent->page,
        PageCache::PAGE_SIZE, PageCache::PAGE_SIZE * page_index);
    assert(cnt > 0);
    ent->wait = false;
  }
  return new Slice(ent->page + (off % PageCache::PAGE_SIZE), size, true);
}

void LocalFileSet::Read(file_t file, size_t size, off_t off, AIO* aio) {
  file_set_[file - 1].Open();

  int page_index = off / PageCache::PAGE_SIZE;
  PageCache::Entry* ent = pagecache_.Lookup(PageHash_(file, page_index));
  aio->SetReadSizeAndOff(size, off % PageCache::PAGE_SIZE);
  if (ent == nullptr) {
    ent = pagecache_.NewEntry(PageHash_(file, page_index));
    aio->SetOpcode(IOCB_CMD_PREAD);
    aio->SetBuf(ent->page);
    aio->SetFileIO(file_set_[file - 1].fd, PageCache::PAGE_SIZE, PageCache::PAGE_SIZE * page_index);
    aio->AddCallback(pagecache_.MakeReadCallback(ent));
    aio->Submit();
  } else {
    aio->SetBuf(ent->page);
    aio->FakeSubmit();
  }
}

void LocalFileSet::Write(file_t file, const Slice* data,
                         off_t off, AIO* aio) {
  file_set_[file - 1].Open();

  aio->SetOpcode(IOCB_CMD_PWRITE);
  aio->SetBuf(const_cast<char*>(data->data()));
  aio->SetFileIO(file_set_[file - 1].fd, data->size(), off);

  int page_index = off / PageCache::PAGE_SIZE;
  PageCache::Entry* ent = pagecache_.Lookup(PageHash_(file, page_index));
  if (ent) {
    aio->AddCallback(pagecache_.MakeWriteCallback(ent, data,
                     off % PageCache::PAGE_SIZE));
  } else {
    delete data;
  }
  aio->Submit();
}

} // namespace kvengine