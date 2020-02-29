#ifndef KTABLEFS_UTIL_HASH_H_
#define KTABLEFS_UTIL_HASH_H_

#include <stdint.h>

typedef uint64_t hash_t;

hash_t page_hash(int fd, int page_index);
void get_page_from_hash(hash_t hash, int* fd, int* page_index);
uint32_t dir_name_hash(const char* name, size_t len);
uint32_t file_name_hash(const char* name, size_t len);

#endif // KTABLEFS_UTIL_HASH_H_