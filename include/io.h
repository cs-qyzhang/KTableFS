#ifndef KTABLEFS_IO_H_
#define KTABLEFS_IO_H_

#include <sys/types.h>
#include <fuse3/fuse_lowlevel.h>

struct kfs_file_handle;
struct kv_event;

void io_init();
void io_destroy();
void allocate_aggregation_slab(uint32_t* file_no, uint32_t* file_idx);
int write_file(fuse_req_t req, struct kfs_file_handle* handle, const char* data, size_t size, off_t offset);
void write_file_buf(fuse_req_t req, struct kfs_file_handle* handle, struct fuse_bufvec* in_buf, off_t offset);
int read_file(fuse_req_t req, struct kfs_file_handle* handle, size_t size, off_t offset);
void remove_file(struct kfs_file_handle* handle);

#endif // KTABLEFS_IO_H_