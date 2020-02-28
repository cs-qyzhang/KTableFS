#include <sys/types.h>
#include <sys/stat.h>
#include "file.h"

ino_t file_ino(struct kfs_file_handle* file) {
  return file->file.stat.st_ino;
}