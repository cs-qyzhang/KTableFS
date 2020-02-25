/*
 * dcache.c
 * 
 * in-memory dentry cache
 * 
 */
#include "dcache.h"
#include "arena.h"
#include "index.h"

struct dcache {
  struct index* index;
};

ino_t dcache_lookup(const char* dir) {

}