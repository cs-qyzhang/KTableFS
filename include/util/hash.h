#ifndef KTABLEFS_UTIL_HASH_H_
#define KTABLEFS_UTIL_HASH_H_

#include <stdint.h>

typedef uint64_t hash_t;

hash_t page_hash(int fd, int page_index);
void get_page_from_hash(hash_t hash, int* fd, int* page_index);

#endif // KTABLEFS_UTIL_HASH_H_