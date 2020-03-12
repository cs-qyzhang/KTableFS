#define _GNU_SOURCE
#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <fuse3/fuse_opt.h>
#include <dirent.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include "options.h"
#include "ktablefs_config.h"
#include "debug.h"

#ifdef DEBUG
char log_path[256];
int log_fd;

static void print_info(const char* func, const char* path) {
  char buf[256];
  int len = sprintf(buf, "%s %s pid: %d, tid: %d\n", func, path, getpid(), gettid());
  lseek(log_fd, 0, SEEK_END);
  write(log_fd, buf, len);
  printf("%s", buf);
}
#else // DEBUG
static void print_info(const char* func, const char* path) {
}
#endif

struct options options;

#define OPTION(t, p)  { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
  OPTION("--datadir=%s", datadir),
  OPTION("-h", show_help),
  OPTION("--help", show_help),
  FUSE_OPT_END
};

void* kfs_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
#ifdef DEBUG
  log_fd = open(log_path, O_WRONLY);
  char buf[256];
  int len = sprintf(buf, "init pid: %d, tid: %d\n", getpid(), gettid());
  lseek(log_fd, 0, SEEK_END);
  write(log_fd, buf, len);
#endif

  (void)conn;
  (void)cfg;

  return NULL;
}

static void get_ext4_path(char* fpath, const char* path) {
  if (path[1] == '\0')
    sprintf(fpath, "%s", options.datadir);
  else
    sprintf(fpath, "%s%s", options.datadir, path);
}

void kfs_destroy(void* private_data) {
  print_info("destroy", "");
}

int kfs_mknod(const char* path, mode_t mode, dev_t dev) {
  print_info("mkmod", path);

  int ret;
  char fpath[PATH_MAX];
  get_ext4_path(fpath, path);
  if (S_ISREG(mode)) {
    ret = open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode);
    if (ret >= 0)
      ret = close(ret);
  } else if (S_ISFIFO(mode))
    ret = mkfifo(fpath, mode);
  else
    ret = mknod(fpath, mode, dev);
  return ret;
}

/*
 * Set fi->fh to the file handle.
 */
int kfs_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
  print_info("create", path);

  int fd;
  char fpath[PATH_MAX];

  get_ext4_path(fpath, path);
  fd = open(fpath, fi->flags | O_CREAT, mode & 0777);

  if (fd < 0) {
    perror(strerror(errno));
    return -ENOENT;
  }

  fi->fh = fd;
  return 0;
}

/*
 * Set fi->fh to the file handle.
 * If file doesn't exist, return -ENOENT.
 * Don't check creation (O_CREAT, O_EXCL, O_NOCTTY) flags,
 * because they have been filtered out / handled by the kernel.
 */
int kfs_open(const char* path, struct fuse_file_info* fi) {
  print_info("open", path);

  int fd;
  char fpath[PATH_MAX];

  get_ext4_path(fpath, path);
  fd = open(fpath, fi->flags);

  if (fd < 0)
    return -ENOENT;

  fi->fh = fd;
  return 0;
}

int kfs_release(const char* path, struct fuse_file_info* fi) {
  print_info("release", path);

  close(fi->fh);
  return 0;
}

/*
 * return read byte.
 * change atime.
 */
int kfs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
  print_info("read", path);

  return pread(fi->fh, buf, size, offset);
}

/*
 * return write byte.
 * change mtime and ctime.
 */
int kfs_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
  print_info("write", path);

  return pwrite(fi->fh, buf, size, offset);
}

int kfs_unlink(const char* path) {
  print_info("unlink", path);

  char fpath[PATH_MAX];
  get_ext4_path(fpath, path);
  return unlink(fpath);
}

int kfs_getattr(const char* path, struct stat* st, struct fuse_file_info *fi) {
  print_info("getattr", path);

  char fpath[PATH_MAX];
  get_ext4_path(fpath, path);

  int ret = lstat(fpath, st);
  if (ret < 0)
    return -ENOENT;
  return ret;
}

int kfs_mkdir(const char* path, mode_t mode) {
  print_info("mkdir", path);

  char fpath[PATH_MAX];
  get_ext4_path(fpath, path);
  return mkdir(fpath, mode);
}

int kfs_rmdir(const char* path) {
  print_info("rmdir", path);

  char fpath[PATH_MAX];
  get_ext4_path(fpath, path);
  return rmdir(fpath);
}

int kfs_opendir(const char* path, struct fuse_file_info* fi) {
  print_info("opendir", path);

  DIR *dp;
  char fpath[PATH_MAX];

  get_ext4_path(fpath, path);

  dp = opendir(fpath);
  if (dp == NULL)
    return -ENOENT;
  fi->fh = (intptr_t) dp;
  return 0;
}

int kfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset,
    struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
  print_info("readdir", path);

  DIR *dp;
  struct dirent *de;
    
  dp = (DIR *) (uintptr_t) fi->fh;

  de = readdir(dp);
  if (de == 0) {
    return -ENOENT;
  }

  do {
    if (filler(buf, de->d_name, NULL, 0, 0) != 0) {
      return -ENOMEM;
    }
  } while ((de = readdir(dp)) != NULL);

  return 0;
}

int kfs_releasedir(const char* path, struct fuse_file_info* fi) {
  print_info("releasedir", path);

  closedir((DIR*)fi->fh);
  return 0;
}

int kfs_link(const char* src, const char* dst) {
  print_info("link dst", dst);
  print_info("link src", src);

  char fdst[PATH_MAX];
  get_ext4_path(fdst, dst);
  char fsrc[PATH_MAX];
  get_ext4_path(fsrc, src);
  return link(fsrc, fdst);
}

int kfs_symlink(const char* src, const char* dst) {
  print_info("symlink", src);
  print_info("symlink", dst);

  char fdst[PATH_MAX];
  get_ext4_path(fdst, dst);
  return symlink(src, fdst);
}

int kfs_readlink(const char* path, char* buf, size_t size) {
  print_info("readlink", path);

  char fpath[PATH_MAX];
  get_ext4_path(fpath, path);
  int ret = readlink(fpath, buf, size - 1);
  if (ret >= 0)
    buf[ret] = '\0';
  return 0;
}

static struct fuse_operations kfs_operations = {
  .init       = kfs_init,
  .getattr    = kfs_getattr,
  .opendir    = kfs_opendir,
  .readdir    = kfs_readdir,
  .releasedir = kfs_releasedir,
  .mkdir      = kfs_mkdir,
  .rmdir      = kfs_rmdir,
  .rename     = NULL,

  .link       = kfs_link,
  .symlink    = kfs_symlink,
  .readlink   = kfs_readlink,

  .open       = kfs_open,
  .read       = kfs_read,
  .write      = kfs_write,
  .create     = kfs_create,
  .mknod      = 0,
  .unlink     = kfs_unlink,
  .release    = kfs_release,
  .chmod      = NULL,
  .chown      = NULL,

  .truncate   = NULL,
  .access     = NULL,
  .utimens    = NULL,
  .destroy    = kfs_destroy,
};

void show_help(const char* progname) {
  printf("usage: %s [options] --datadir=<datadir> mountdir\n", progname);
}

int main(int argc, char** argv) {
  options.mountdir = strdup(argv[argc - 1]);
  options.workdir = getcwd(NULL, 0);

  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  /* Set defaults -- we have to use strdup so that
     fuse_opt_parse can free the defaults if other
     values are specified */
  // options.filename = strdup("hello");
  // options.contents = strdup("Hello World!\n");

  /* Parse options */
  if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
    return 1;

#ifdef DEBUG
  printf("workdir: %s, datadir: %s, mountdir: %s\n", options.workdir,
      options.datadir, options.mountdir);

  sprintf(log_path, "%s/log", options.workdir);
  int fd = open(log_path, O_CREAT | O_WRONLY | O_TRUNC, 0666);
  char buf[256];
  int len = sprintf(buf, "main pid: %d, tid: %d\n", getpid(), gettid());
  pwrite(fd, buf, len, 0);
  close(fd);
#endif

  /* When --help is specified, first print our own file-system
     specific help text, then signal fuse_main to show
     additional help (by adding `--help` to the options again)
     without usage: line (by setting argv[0] to the empty
     string) */
  if (options.show_help) {
    show_help(argv[0]);
    assert(fuse_opt_add_arg(&args, "--help") == 0);
    args.argv[0][0] = '\0';
  }

  ret = fuse_main(args.argc, args.argv, &kfs_operations, NULL);
  fuse_opt_free_args(&args);
  return ret;
}