#ifndef KTABLEFS_KTABLEFS_H_
#define KTABLEFS_KTABLEFS_H_

#include <atomic>
#include "fuse_header.h"
#include "kvengine/db.h"
#include "file.h"
#include "file_data.h"

namespace ktablefs {

class KTableFS {
 private:
  std::string data_dir_;
  kvengine::DB* db_;
  FileData* file_data_;
  FileHandle* root_;
  std::atomic<uint64_t> cur_ino_;

  FileHandle* Handle(fuse_req_t req, fuse_ino_t ino) {
    if (ino == FUSE_ROOT_ID)
      return root_;
    else
      return reinterpret_cast<FileHandle*>(ino);
  }

  ino_t NewIno() { return ++cur_ino_; }

  friend class FileHandle;

 public:
  static struct options {
    const char* mountdir;
    const char* datadir;
    const char* workdir;
    int show_help;
  } option;

  static uid_t st_uid;
  static gid_t st_gid;
  static dev_t st_dev;
  static dev_t st_rdev;
  static blksize_t st_blksize;
  static double timeout;

  static KTableFS* fs;

  KTableFS(std::string meta_dir);
  KTableFS(const KTableFS&) = delete;
  KTableFS(KTableFS&&) = delete;

  static void ReadDir();
  static void Lookup(fuse_req_t, fuse_ino_t, const char*);
  static void Mknod(fuse_req_t, fuse_ino_t, const char*, mode_t, dev_t);
  static void Open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);
  static void Write(fuse_req_t req, fuse_ino_t ino, const char *buf,
		                size_t size, off_t off, struct fuse_file_info *fi);
	static void Read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
		               struct fuse_file_info *fi);
  static void GetAttr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi);

  static void Init(void *userdata, struct fuse_conn_info *conn);
  static void Destroy(void *userdata);
};

} // namespace ktablefs

#endif // KTABLEFS_KTABLEFS_H_