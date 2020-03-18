#ifndef KTABLEFS_FILE_H_
#define KTABLEFS_FILE_H_

#include <sys/stat.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>

// bit set
#define KFS_REG       1
#define KFS_HARDLINK  2
#define KFS_SYMLINK   4

struct kfs_stat {
  uint32_t atime;
  uint32_t mtime;
  uint32_t ctime;
  mode_t st_mode;
  uid_t st_uid;
  gid_t st_gid;
  nlink_t st_nlink;
  ino_t st_ino;
  off_t st_size;
};

/*
 * the struct is designed that it has no padding, and it's size is 64,
 * which is as same as cache line.
 */
struct kfs_file {
  uint16_t type;
  uint16_t next;
  uint32_t slab_file_no;
  uint32_t slab_file_idx;
  uint32_t big_file_no;
  union {
    struct kfs_stat stat;
    char blob[1];
  };
};

struct key;

struct kfs_file_handle {
  struct kfs_file* file;
  struct key* key;
  int big_file_fd;
};

extern struct stat* root_stat;

static inline struct kfs_stat* file_stat(struct kfs_file_handle* handle) {
  return &handle->file->stat;
}

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
  handle->file->stat.atime = time(NULL);
}

static inline void update_ctime(struct kfs_file_handle* handle) {
  handle->file->stat.ctime = time(NULL);
}

static inline void update_mtime(struct kfs_file_handle* handle) {
  handle->file->stat.mtime = time(NULL);
}

static inline void file_fill_stat(struct stat* dst, struct kfs_stat* src) {
  //memset(dst, 0, sizeof(*dst));
  dst->st_blksize = root_stat->st_blksize;
  dst->st_dev = root_stat->st_dev;
  dst->st_rdev = root_stat->st_rdev;

  dst->st_atime = (time_t)src->atime;
  dst->st_mtime = (time_t)src->mtime;
  dst->st_ctime = (time_t)src->ctime;
  dst->st_mode = src->st_mode;
  dst->st_uid = src->st_uid;
  dst->st_gid = src->st_gid;
  dst->st_nlink = src->st_nlink;
  dst->st_ino = src->st_ino;
  dst->st_size = src->st_size;

  dst->st_blocks = (dst->st_size + dst->st_blksize - 1) / dst->st_blksize;
}

#endif // KTABLEFS_FILE_H_