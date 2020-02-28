#define _GNU_SOURCE
#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include "kvengine/kvengine.h"
#include "kv_impl.h"
#include "ktablefs_config.h"

static struct options {
  const char* mountdir;
  const char* datadir;
  int show_help;
} options;

#define OPTION(t, p)  { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
  OPTION("--mountdir=%s", mountdir),
  OPTION("--datadir=%s", datadir),
  OPTION("-h", show_help),
  OPTION("--help", show_help),
  FUSE_OPT_END
};

void* kfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
  (void)conn;
  (void)cfg;

  struct kv_options* kv_options = malloc(sizeof(*kv_options));
  kv_options->slab_dir = strdup(options.datadir);
  kv_options->thread_nr = KVENGINE_THREAD_NR;
  kv_init(kv_options);

  return NULL;
}

int kfs_open(const char *path, struct fuse_file_info *fi) {
  size_t len = strlen(path);
  char* end = path + (len - 1);
  while (*end != '/')
    end--;

  struct kfs_file_handle* dir;
  dir = dcache_lookup(path, (end - path) + 1);

  struct kv_request* req = malloc(sizeof(*req));
  req->key = malloc(sizeof(*req->key));
  req->key.dir_ino = file_ino(dir);
  req->key.length = len - (end - path) - 1;
  req->key.data = malloc(req->key.length + 1);
  memcpy(req->key.data, end + 1, req->key.length);
  req->key.data[req->key.length - 1] = '\0';
  req->key.hash = file_name_hash(end + 1, req->key.length);
  req->type = GET;

  int sequence = kv_submit(req);
  struct kv_event* event = kv_getevent(sequence);
  fi->fh = (uint64_t)event->value->file;
  return 0;
}

int kfs_read(const char *, char *, size_t, off_t, struct fuse_file_info *) {

}

int kfs_write(const char *, const char *, size_t, off_t, struct fuse_file_info *) {

}

int kfs_getattr(const char* path, const char* , char *, size_t) {

}

static struct fuse_operations kfs_operations = {
  .init       = kfs_init;
  .getattr    = NULL;
  .opendir    = NULL;
  .readdir    = NULL;
  .releasedir = NULL;
  .mkdir      = NULL;
  .rmdir      = NULL;
  .rename     = NULL;

  .symlink    = NULL;
  .readlink   = NULL;

  .open       = kfs_open;
  .read       = kfs_read;
  .write      = kfs_write;
  .mknod      = NULL;
  .unlink     = NULL;
  .release    = NULL;
  .chmod      = NULL;
  .chown      = NULL;

  .truncate   = NULL;
  .access     = NULL;
  .utimens    = NULL;
  .destroy    = NULL;
};

int main(int argc, char** argv) {
  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  /* Set defaults -- we have to use strdup so that
     fuse_opt_parse can free the defaults if other
     values are specified */
  options.filename = strdup("hello");
  options.contents = strdup("Hello World!\n");

  /* Parse options */
  if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
    return 1;

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