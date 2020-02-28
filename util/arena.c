/*
 * arena.c
 *
 * inspired by LevelDB's Arena
 *
 * use arena to reduce the number of small memory malloc
 */
#include <stdlib.h>
#include <stdint.h>
#include "util/arena.h"
#include "ktablefs_config.h"
#include "debug.h"

struct arena {
  // current block index
  size_t alloc_blk_idx_;
  // point to avaliable memeory
  char*  alloc_ptr_;
  // avaliable bytes in current block
  size_t alloc_bytes_remaining_;
  size_t max_blocks_;

  // array of blocks
  char** blocks_;
};

// create an new arena
struct arena* arena_new() {
  struct arena* arena_ = (struct arena*)malloc(sizeof(struct arena));
  arena_->blocks_ = (char**)malloc(sizeof(char*) * ARENA_NB_BLOCKS);
  Assert(arena_->blocks_);
  arena_->blocks_[0] = (char*)malloc(ARENA_BLOCK_SIZE);
  Assert(arena_->blocks_[0]);
  arena_->alloc_blk_idx_ = 0;
  arena_->alloc_ptr_ = arena_->blocks_[0];
  arena_->max_blocks_ = ARENA_NB_BLOCKS;
  arena_->alloc_bytes_remaining_ = ARENA_BLOCK_SIZE;
  return arena_;
}

// destroy an arena
void arena_destroy(struct arena* arena_) {
  for (int i = 0; i <= arena_->alloc_blk_idx_; ++i) {
    free(arena_->blocks_[i]);
  }
  free(arena_->blocks_);
  free(arena_);
}

static void arena_alloc_block(struct arena* arena_, size_t size) {
  if (arena_->alloc_blk_idx_ == arena_->max_blocks_ - 1) {
    arena_->blocks_ = (char**)realloc(arena_->blocks_,
        (arena_->max_blocks_ + ARENA_NB_BLOCKS) * ARENA_BLOCK_SIZE);
    Assert(arena_->blocks_);
    arena_->max_blocks_ += ARENA_NB_BLOCKS;
  }
  arena_->blocks_[++arena_->alloc_blk_idx_] = (char *)malloc(size);
  Assert(arena_->blocks_[arena_->alloc_blk_idx_]);
}

static void arena_alloc_block_aligned(struct arena* arena_, size_t size, size_t align) {
  if (arena_->alloc_blk_idx_ == arena_->max_blocks_ - 1) {
    arena_->blocks_ = (char**)realloc(arena_->blocks_,
        (arena_->max_blocks_ + ARENA_NB_BLOCKS) * ARENA_BLOCK_SIZE);
    Assert(arena_->blocks_);
    arena_->max_blocks_ += ARENA_NB_BLOCKS;
  }
  arena_->blocks_[++arena_->alloc_blk_idx_] = (char *)aligned_alloc(align, size);
  Assert(arena_->blocks_[arena_->alloc_blk_idx_]);
}

// allocate memory in arena
void* arena_allocate(struct arena* arena_, size_t size) {
  Assert(size > 0);
  // if size is bigger than block size's 1/4, we allocate
  // a new block to store it.
  if (size >= ARENA_BLOCK_SIZE / 4) {
    arena_alloc_block(arena_, size);
    // sawp newly alloc blocks and last blocks, because last blocks
    // maybe remaining some space, we can use it in the next allocation.
    char* tmp = arena_->blocks_[arena_->alloc_blk_idx_ - 1];
    arena_->blocks_[arena_->alloc_blk_idx_ - 1] = arena_->blocks_[arena_->alloc_blk_idx_];
    arena_->blocks_[arena_->alloc_blk_idx_] = tmp;
    return arena_->blocks_[arena_->alloc_blk_idx_ - 1];
  } else {
    if (arena_->alloc_bytes_remaining_ >= size) {
      void* ret = arena_->alloc_ptr_;
      arena_->alloc_ptr_ += size;
      arena_->alloc_bytes_remaining_ -= size;
      return ret;
    } else {
      arena_alloc_block(arena_, ARENA_BLOCK_SIZE);
      arena_->alloc_ptr_ = arena_->blocks_[arena_->alloc_blk_idx_] + size;
      arena_->alloc_bytes_remaining_ = ARENA_BLOCK_SIZE - size;
      return arena_->blocks_[arena_->alloc_blk_idx_];
    }
  }
}

// allocate aligned memory in arena
void* arena_allocate_aligned(struct arena* arena_, size_t size, size_t align) {
  Assert(size > 0);
  Assert((align & (align - 1)) == 0);
  if (size >= ARENA_BLOCK_SIZE / 4) {
    arena_alloc_block_aligned(arena_, size, align);
    // sawp newly alloc blocks and last blocks, because last blocks
    // maybe remaining some space, we can use it in the next allocation.
    char* tmp = arena_->blocks_[arena_->alloc_blk_idx_ - 1];
    arena_->blocks_[arena_->alloc_blk_idx_ - 1] = arena_->blocks_[arena_->alloc_blk_idx_];
    arena_->blocks_[arena_->alloc_blk_idx_] = tmp;
    return arena_->blocks_[arena_->alloc_blk_idx_ - 1];
  } else {
    // change alloc_ptr_ to next aligned position, if next aligned
    // position is out-of-block, then simply make alloc_bytes_remaining_
    // be 0 and we will malloc a new aligned block later.
    if ((uintptr_t)arena_->alloc_ptr_ & (align - 1)) {
      char* new_ptr = (void*)(((uintptr_t)arena_->alloc_ptr_ + align) & ((uintptr_t)-1 ^ (align - 1)));
      if (new_ptr - arena_->alloc_ptr_ < arena_->alloc_bytes_remaining_) {
        arena_->alloc_bytes_remaining_ -= new_ptr - arena_->alloc_ptr_;
        arena_->alloc_ptr_ = new_ptr;
      } else {
        arena_->alloc_bytes_remaining_ = 0;
      }
    }
    // now alloc_ptr_ is aligned
    if (arena_->alloc_bytes_remaining_ >= size) {
      void* ret = arena_->alloc_ptr_;
      arena_->alloc_ptr_ += size;
      arena_->alloc_bytes_remaining_ -= size;
      return ret;
    } else {
      arena_alloc_block_aligned(arena_, ARENA_BLOCK_SIZE, align);
      arena_->alloc_ptr_ = arena_->blocks_[arena_->alloc_blk_idx_] + size;
      arena_->alloc_bytes_remaining_ = ARENA_BLOCK_SIZE - size;
      return arena_->blocks_[arena_->alloc_blk_idx_];
    }
  }
}