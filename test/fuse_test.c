#define _GNU_SOURCE
#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>

/*
 * Command line options
 *
 * We can't set default values for the char* fields here because
 * fuse_opt_parse would attempt to free() them when the user specifies
 * different values on the command line.
 */
static struct kv_optionss {
  const char *filename;
  const char *contents;
  int show_help;
} options;

#define OPTION(t, p)                           \
    { t, offsetof(struct kv_optionss, p), 1 }
static const struct fuse_opt option_spec[] = {
  OPTION("--name=%s", filename),
  OPTION("--contents=%s", contents),
  OPTION("-h", show_help),
  OPTION("--help", show_help),
  FUSE_OPT_END
};

static void print_info(const char* func) {
  int fd = open("/home/ubuntu/fuse.txt", O_WRONLY);
  char buf[256];
  int len = sprintf(buf, "%s pid: %d, tid: %d\n", func, getpid(), gettid());
  lseek(fd, 0, SEEK_END);
  write(fd, buf, len);
  close(fd);
}

static void *hello_init(struct fuse_conn_info *conn,
      struct fuse_config *cfg) {
  print_info("init");

  (void) conn;
  cfg->kernel_cache = 1;
  return NULL;
}

static int hello_getattr(const char *path, struct stat *stbuf,
       struct fuse_file_info *fi) {
  print_info("getattr");

  (void) fi;
  int res = 0;

  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
  } else if (strcmp(path+1, options.filename) == 0) {
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = strlen(options.contents);
  } else
    res = -ENOENT;

  return res;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
       off_t offset, struct fuse_file_info *fi,
       enum fuse_readdir_flags flags) {
  print_info("readdir");

  (void) offset;
  (void) fi;
  (void) flags;

  if (strcmp(path, "/") != 0)
    return -ENOENT;

  filler(buf, ".", NULL, 0, 0);
  filler(buf, "..", NULL, 0, 0);
  filler(buf, options.filename, NULL, 0, 0);

  return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi) {
  print_info("open");

  if (strcmp(path+1, options.filename) != 0)
    return -ENOENT;

  if ((fi->flags & O_ACCMODE) != O_RDONLY)
    return -EACCES;

  return 0;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
          struct fuse_file_info *fi) {
  print_info("read");

  size_t len;
  (void) fi;
  if(strcmp(path+1, options.filename) != 0)
    return -ENOENT;

  len = strlen(options.contents);
  if (offset < len) {
    if (offset + size > len)
      size = len - offset;
    memcpy(buf, options.contents + offset, size);
  } else
    size = 0;

  return size;
}

static const struct fuse_operations hello_oper = {
  .init    = hello_init,
  .getattr = hello_getattr,
  .readdir = hello_readdir,
  .open    = hello_open,
  .read    = hello_read,
};

void show_help(const char* progname) {
  printf("usage: %s [options] <mountpoint>\n\n", progname);
  printf("File-system specific options:\n"
         "    --name=<s>          Name of the \"hello\" file\n"
         "                        (default: \"hello\")\n"
         "    --contents=<s>      Contents \"hello\" file\n"
         "                        (default \"Hello, World!\\n\")\n"
         "\n");
}

int main(int argc, char** argv) {
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  int fd = open("fuse.txt", O_CREAT | O_WRONLY | O_TRUNC, 0666);
  char buf[256];
  int len = sprintf(buf, "main pid: %d, tid: %d\n", getpid(), gettid());
  pwrite(fd, buf, len, 0);
  close(fd);

  /*
   * Set defaults -- we have to use strdup so that
   * fuse_opt_parse can free the defaults if other
   * values are specified
   */
  options.filename = strdup("hello");
  options.contents = strdup("Hello World!\n");

  /* Parse options */
  if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
    return 1;
  
  if (options.show_help) {
    show_help(argv[0]);
    assert(fuse_opt_add_arg(&args, "--help") == 0);
    args.argv[0][0] = '\0';
  }

  int ret = fuse_main(args.argc, args.argv, &hello_oper, NULL);
  fuse_opt_free_args(&args);
  return ret;
}