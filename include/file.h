#ifndef KTABLEFS_FILE_H_
#define KTABLEFS_FILE_H_

#include <sys/stat.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

struct kfs_file {
  struct stat stat;
  uint32_t slab_file_no;
  uint32_t slab_file_idx;
  uint32_t big_file_no;
};

struct kfs_file_handle {
  struct kfs_file* file;
  off_t offset;
  int big_file_fd;
};

static inline ino_t file_ino(struct kfs_file_handle* handle) {
  return handle->file->stat.st_ino;
}

static inline void file_set_ino(struct kfs_file_handle* handle, ino_t ino) {
  handle->file->stat.st_ino = ino;
}

static inline size_t file_size(struct kfs_file_handle* handle) {
  return handle->file->stat.st_size;
}

static inline void file_set_size(struct kfs_file_handle* handle, size_t size) {
  handle->file->stat.st_size = size;
}

static inline void update_atime(struct kfs_file_handle* handle) {
  handle->file->stat.st_atime = time(NULL);
}

static inline void update_ctime(struct kfs_file_handle* handle) {
  handle->file->stat.st_ctime = time(NULL);
}

static inline void update_mtime(struct kfs_file_handle* handle) {
  handle->file->stat.st_mtime = time(NULL);
}

#endif // KTABLEFS_FILE_H_