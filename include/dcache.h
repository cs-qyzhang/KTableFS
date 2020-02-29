#ifndef KTABLEFS_DCACHE_H_
#define KTABLEFS_DCACHE_H_

#include <sys/types.h>

struct dentrycache;

struct dentrycache* dcache_new();
void dcache_destroy(struct dentrycache* dcache);
ino_t dcache_lookup(struct dentrycache* dcache, const char* dir, size_t len);

#endif // KTABLEFS_DCACHE_H_