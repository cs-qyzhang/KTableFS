#ifndef KTABLEFS_UTIL_FREELIST_H_
#define KTABLEFS_UTIL_FREELIST_H_

struct freelist;
struct arena;

struct freelist* freelist_new(struct arena* arena);
void freelist_add(struct freelist* list, void* data);
void* freelist_get(struct freelist* list);

#endif // KTABLEFS_UTIL_FREELIST_H_