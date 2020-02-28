#ifndef KTABLEFS_FILE_H_
#define KTABLEFS_FILE_H_

#include <sys/stat.h>
#include <stdint.h>
#include <stddef.h>

struct kfs_file {
  struct stat stat;
  uint32_t aggregation_file_no;
  uint32_t aggregation_file_idx;
  uint32_t big_file_no;
};

struct kfs_file_handle {
  struct kfs_file* file;
  off_t offset;
  int big_file_fd;
};

ino_t file_ino(struct kfs_file_handle* file);

#endif // KTABLEFS_FILE_H_