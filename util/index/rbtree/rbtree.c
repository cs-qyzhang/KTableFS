#include <stdlib.h>
#include <stdint.h>
#include "util/memslab.h"
#include "util/rbtree.h"
#include "util/hash_table.h"
#include "debug.h"

struct rbtree_node {
  struct rbtree_node* parent;
  struct rbtree_node* left;
  struct rbtree_node* right;
  void* key;
  void* value;
  enum color {
    BLACK,
    RED
  } color;
};

struct rbtree {
  struct rbtree_node* root;
  struct memslab* node_slab;
  struct rbtree_node* nil;
  struct hash_table* hash_table;
  key_cmp_t key_cmp;
  size_t size;
};

// #define key_less(a, b)    (key_cmp((a), (b)) < 0)
// #define key_greater(a, b) (key_cmp((a), (b)) > 0)
// #define key_equal(a, b)   (key_cmp((a), (b)) == 0)
#define key_less(a, b)    ((*(uint64_t*)(a) < *(uint64_t*)(b)) ? 1 : (*(uint64_t*)(a) == *(uint64_t*)(b)) ? (key_cmp((a), (b)) < 0) : 0)
#define key_greater(a, b) ((*(uint64_t*)(a) > *(uint64_t*)(b)) ? 1 : (*(uint64_t*)(a) == *(uint64_t*)(b)) ? (key_cmp((a), (b)) > 0) : 0)
#define key_equal(a, b)   ((*(uint64_t*)(a) == *(uint64_t*)(b)) ? (key_cmp((a), (b)) == 0) : 0)

static __always_inline struct rbtree_node* rb_node_new(struct rbtree* T, void* key, void* value) {
  struct rbtree_node* new = memslab_alloc(T->node_slab);
  new->key = key;
  new->value = value;
  return new;
}

static __always_inline void rb_node_free(struct rbtree* T, struct rbtree_node* x) {
  memslab_free(T->node_slab, x);
}

#ifdef DEBUG

static int rb_valid_recurse(struct rbtree* T, struct rbtree_node* root) {
  if (root == T->nil)
    return 1;
  int left_depth = rb_valid_recurse(T, root->left);
  int right_depth = rb_valid_recurse(T, root->right);
  assert(left_depth == right_depth);
  if (root->color == RED) {
    assert(root->left->color == BLACK);
    assert(root->right->color == BLACK);
    return left_depth;
  } else {
    return left_depth + 1;
  }
}

static void rb_valid(struct rbtree* T) {
  assert(T->root->color == BLACK);
  rb_valid_recurse(T, T->root);
}

#endif

struct rbtree* rbtree_new(key_cmp_t key_cmp) {
  struct rbtree* T = malloc(sizeof(*T));
  T->node_slab = memslab_new(sizeof(struct rbtree_node));
  T->hash_table = hash_table_new(8192);
  T->size = 0;
  T->key_cmp = key_cmp;
  T->nil = rb_node_new(T, NULL, NULL);
  T->nil->color = BLACK;
  T->nil->left = NULL;
  T->nil->right = NULL;
  T->root = T->nil;
  return T;
}

void rbtree_destroy(struct rbtree* T) {
  memslab_destroy(T->node_slab);
  hash_table_destroy(T->hash_table);
  free(T);
}

static void left_rotate(struct rbtree* T, struct rbtree_node* x) {
  struct rbtree_node* y = x->right;
  x->right = y->left;
  if (y->left != T->nil)
    y->left->parent = x;
  y->parent = x->parent;
  if (x->parent == T->nil)
    T->root = y;
  else if (x == x->parent->left)
    x->parent->left = y;
  else
    x->parent->right = y;
  y->left = x;
  x->parent = y;
}

static void right_rotate(struct rbtree* T, struct rbtree_node* x) {
  struct rbtree_node* y = x->left;
  x->left = y->right;
  if (y->right != T->nil)
    y->right->parent = x;
  y->parent = x->parent;
  if (x->parent == T->nil)
    T->root = y;
  else if (x == x->parent->left)
    x->parent->left = y;
  else
    x->parent->right = y;
  y->right = x;
  x->parent = y;
}

static void rb_insert_fixup(struct rbtree* T, struct rbtree_node* z) {
  while (z->parent->color == RED) {
    if (z->parent == z->parent->parent->left) {
      struct rbtree_node* y = z->parent->parent->right;
      if (y->color == RED) {
        z->parent->color = BLACK;
        y->color = BLACK;
        z->parent->parent->color = RED;
        z = z->parent->parent;
      } else {
        if (z == z->parent->right) {
          z = z->parent;
          left_rotate(T, z);
        }
        z->parent->color = BLACK;
        z->parent->parent->color = RED;
        right_rotate(T, z->parent->parent);
      }
    } else {
      struct rbtree_node* y = z->parent->parent->left;
      if (y->color == RED) {
        z->parent->color = BLACK;
        y->color = BLACK;
        z->parent->parent->color = RED;
        z = z->parent->parent;
      } else {
        if (z == z->parent->left) {
          z = z->parent;
          right_rotate(T, z);
        }
        z->parent->color = BLACK;
        z->parent->parent->color = RED;
        left_rotate(T, z->parent->parent);
      }
    }
  }
  T->root->color = BLACK;
}

int rbtree_insert(struct rbtree* T, void* key, void* value) {
  struct rbtree_node* z = rb_node_new(T, key, value);
  struct rbtree_node* y = T->nil;
  struct rbtree_node* x = T->root;
  key_cmp_t key_cmp = T->key_cmp;
  while (x != T->nil) {
    y = x;
    if (key_less(z->key, x->key)) {
      x = x->left;
    } else if (key_equal(z->key, x->key)) {
      rb_node_free(T, z);
      return -1;
    } else {
      x = x->right;
    }
  }
  z->parent = y;
  if (y == T->nil)
    T->root = z;
  else if (key_less(z->key, y->key))
    y->left = z;
  else
    y->right = z;
  z->left = T->nil;
  z->right = T->nil;
  z->color = RED;
  rb_insert_fixup(T, z);
  T->size += 1;
  // hash_table_insert(T->hash_table, key, value);
  return 0;
}

static inline struct rbtree_node* find_min_node(struct rbtree* T, struct rbtree_node* n) {
  while (n->left != T->nil)
    n = n->left;
  return n;
}

static void transplant(struct rbtree* T, struct rbtree_node* u, struct rbtree_node* v) {
  if (u->parent == T->nil)
    T->root = v;
  else if (u == u->parent->left)
    u->parent->left = v;
  else
    u->parent->right = v;
  if (v)
    v->parent = u->parent;
}

static void rb_delete_fixup(struct rbtree* T, struct rbtree_node* x) {
  while (x != T->root && x->color == BLACK) {
    if (x == x->parent->left) {
      struct rbtree_node* w = x->parent->right;
      if (w->color == RED) {
        w->color = BLACK;
        x->parent->color = RED;
        left_rotate(T, x->parent);
        w = x->parent->right;
      }
      if (w->left->color == BLACK && w->right->color == BLACK) {
        w->color = RED;
        x = x->parent;
      } else {
        if (w->right->color == BLACK) {
          w->left->color = BLACK;
          w->color = RED;
          right_rotate(T, w);
          w = x->parent->right;
        }
        w->color = x->parent->color;
        x->parent->color = BLACK;
        w->right->color = BLACK;
        left_rotate(T, x->parent);
        x = T->root;
      }
    } else {
      struct rbtree_node* w = x->parent->left;
      if (w->color == RED) {
        w->color = BLACK;
        x->parent->color = RED;
        right_rotate(T, x->parent);
        w = x->parent->left;
      }
      if (w->left->color == BLACK && w->right->color == BLACK) {
        w->color = RED;
        x = x->parent;
      } else {
        if (w->left->color == BLACK) {
          w->right->color = BLACK;
          w->color = RED;
          left_rotate(T, w);
          w = x->parent->left;
        }
        w->color = x->parent->color;
        x->parent->color = BLACK;
        w->left->color = BLACK;
        right_rotate(T, x->parent);
        x = T->root;
      }
    }
  }
  x->color = BLACK;
}

static void delete_node(struct rbtree* T, struct rbtree_node* z) {
  struct rbtree_node* y = z;
  enum color y_original_color = y->color;
  struct rbtree_node* x;
  if (z->left == T->nil) {
    x = z->right;
    transplant(T, z, z->right);
  } else if (z->right == T->nil) {
    x = z->left;
    transplant(T, z, z->left);
  } else {
    y = find_min_node(T, z->right);
    y_original_color = y->color;
    x = y->right;
    if (y->parent == z)
      x->parent = y;
    else {
      transplant(T, y, y->right);
      y->right = z->right;
      y->right->parent = y;
    }
    transplant(T, z, y);
    y->left = z->left;
    y->left->parent = y;
    y->color = z->color;
  }
  if (y_original_color == BLACK)
    rb_delete_fixup(T, x);
  rb_node_free(T, z);
  T->size -= 1;
}

int rbtree_remove(struct rbtree* T, void* key) {
  struct rbtree_node* node = T->root;
  key_cmp_t key_cmp = T->key_cmp;
  while (node != T->nil) {
    if (key_less(key, node->key)) {
      node = node->left;
    } else if (key_equal(key, node->key)) {
      // hash_table_remove(T->hash_table, *(hash_t*)key);
      delete_node(T, node);
      return 0;
    } else {
      node = node->right;
    }
  }
  return -1;
}

void* rbtree_lookup(struct rbtree* T, void* key) {
  key_cmp_t key_cmp = T->key_cmp;
  // void* find_key;
  // void* data = hash_table_find(T->hash_table, *(hash_t*)key, &find_key);
  // if (data && key_equal(key, find_key))
  //   return data;
  struct rbtree_node* node = T->root;
  while (node != T->nil) {
    if (key_less(key, node->key))
      node = node->left;
    else if (key_equal(key, node->key)) {
      // hash_table_insert(T->hash_table, key, node->value);
      return node->value;
    }
    else
      node = node->right;
  }
  return NULL;
}

static int rbtree_scan_recurse(struct rbtree* T, struct rbtree_node* root, void* min_key,
                               void* max_key, scan_t scan, void* scan_arg) {
  if (root == T->nil)
    return 0;

  key_cmp_t key_cmp = T->key_cmp;
  if (key_less(root->key, min_key))
    return rbtree_scan_recurse(T, root->right, min_key, max_key, scan, scan_arg);
  else if (key_greater(root->key, max_key) || key_equal(root->key, max_key))
    return rbtree_scan_recurse(T, root->left, min_key, max_key, scan, scan_arg);
  else {
    int count = 0;
    scan(root->key, root->value, scan_arg);
    count++;
    count += rbtree_scan_recurse(T, root->left, min_key, max_key, scan, scan_arg);
    count += rbtree_scan_recurse(T, root->right, min_key, max_key, scan, scan_arg);
    return count;
  }
}

int rbtree_scan(struct rbtree* T, void* min_key, void* max_key,
                scan_t scan, void* scan_arg) {
  return rbtree_scan_recurse(T, T->root, min_key, max_key, scan, scan_arg);
}