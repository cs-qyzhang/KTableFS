#include "util/hash.h"

hash_t page_hash(int fd, int page_index) {
  return ((hash_t)fd << 40U) + (hash_t)page_index;
}

void get_page_from_hash(hash_t hash, int* fd, int* page_index) {
  *page_index = hash & ((1UL << 40) - 1);
  *fd = hash >> 40U;
}