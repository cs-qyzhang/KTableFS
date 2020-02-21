#ifndef KTABLEFS_KVENGINE_HASH_H_
#define KTABLEFS_KVENGINE_HASH_H_

#include <sys/types.h>

typedef u_int64_t hash_t;

hash_t page_hash(int fd, int page_index);
void get_page_from_hash(hash_t hash, int* fd, int* page_index);

#endif // KTABLEFS_KVENGINE_HASH_H_