#include <fuse.h>
#include <stdlib.h>
#include "kvengine/kvengine.h"
#include "ktablefs_config.h"

int kfs_mknod(const char* path, mode_t mode, dev_t dev) {

}

int kfs_open(const char *path, struct fuse_file_info *fi) {
  struct value* file;
  file = cache_lookup(path, strlen(path));
  fi->fh = (uint64_t)file;
  return 0;
}
