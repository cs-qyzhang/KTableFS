#ifndef KTABLEFS_KVENGINE_IO_H_
#define KTABLEFS_KVENGINE_IO_H_

// TODO: hash_t define?
#include "kvengine/hash.h"

int io_write_page(hash_t hash, char* page);
int io_read_page(hash_t hash, char* page);

#endif // KTABLEFS_KVENGINE_IO_H_