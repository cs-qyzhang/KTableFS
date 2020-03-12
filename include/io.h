#ifndef KTABLEFS_IO_H_
#define KTABLEFS_IO_H_

#include <sys/types.h>

struct kfs_file_handle;
struct kv_event;

void io_init();
void io_destroy();
int write_file(struct kfs_file_handle* handle, void* data, size_t size, off_t offset);
int read_file(struct kfs_file_handle* handle, void* data, size_t size, off_t offset);
struct kv_event* create_file(ino_t parent_ino, const char* file_name, mode_t mode, size_t len, uint16_t type);
void remove_file(struct kfs_file_handle* handle);

#endif // KTABLEFS_IO_H_