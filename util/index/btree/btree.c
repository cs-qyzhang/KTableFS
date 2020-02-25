#include <stdlib.h>
#include <assert.h>
#include "util/btree.h"
#include "util/arena.h"

#ifdef DEBUG
#define Assert(expr)  assert(expr)
#else // DEBUG
#define Assert(expr)
#endif

struct btree_node {
  struct btree_node* parent;
  struct btree_node* child;
  // sibling node
  struct btree_node* next;
  struct btree* tree;
  struct pair {
    void * key;
    void* value;
    struct pair* next;
  }* head;
  size_t size;
};

struct btree {
  struct btree_node* root;
  struct arena* arena;
  key_cmp_t key_cmp;
  int order;
  size_t size;
};

#define key_less(a, b)    (node->tree->key_cmp((a), (b)) < 0)
#define key_greater(a, b) (node->tree->key_cmp((a), (b)) > 0)
#define key_equal(a, b)   (node->tree->key_cmp((a), (b)) == 0)

/*
 * return the biggest pair which less than key.
 * if node size is 0 or all pair are bigger than key, return NULL 
 */
static struct pair* btree_node_find(struct btree_node* node, void* key) {
  if (node->head == NULL || key_greater(node->head->key, key)) {
    return NULL;
  }
  struct pair* pos = node->head;
  while (pos->next && key_less(pos->next->key, key)) {
    pos = pos->next;
  }
  return pos;
}

static void btree_node_split(struct btree_node* node);
static int btree_node_insert(struct btree_node* node, struct pair* new_pair) {
  struct pair* pos = btree_node_find(node, new_pair->key);
  if (pos == NULL) {
    // insert pair at head
    new_pair->next = node->head;
    node->head = new_pair;
  } else {
    Assert(pos->next == NULL || !key_equal(pos->next->key, new_pair->key));
    new_pair->next = pos->next;
    pos->next = new_pair;
  }
  node->size += 1;
  btree_node_split(node);
  return 0;
}

static void btree_node_merge(struct btree_node* node);
static int btree_node_remove(struct btree_node* node, void* key) {
  Assert(node->child == NULL);
  if (node->size == 0) {
    return -1;
  }
  if (key_equal(node->head->key, key)) {
    node->head = node->head->next;
    node->size -= 1;
    btree_node_merge(node);
    return 0;
  }

  struct pair* pos = node->head;
  while (pos->next && key_less(pos->next->key, key)) {
    pos = pos->next;
  }
  if (pos->next == NULL || !key_equal(pos->next->key, key)) {
    return -1;
  } else {
    pos->next = pos->next->next;
    node->size -= 1;
    btree_node_merge(node);
    return 0;
  }
}

static void btree_node_split(struct btree_node* node);
/*
 * when child node need split, it will call this function
 * to insert splited new node to parent
 */
static void btree_node_insert_parent(struct btree_node* left_node,
                                     struct btree_node* right_node,
                                     struct pair* mid_pair) {
  right_node->next = left_node->next;
  left_node->next = right_node;
  if (left_node->parent == NULL) {
    // root node split
    struct btree_node* new_root;
    new_root = arena_allocate(left_node->tree->arena, sizeof(*new_root));
    new_root->head = NULL;
    new_root->next = NULL;
    new_root->tree = left_node->tree;
    new_root->child = left_node;
    new_root->parent = NULL;
    new_root->size = 0;
    left_node->tree->root = new_root;
    left_node->parent = new_root;
    right_node->parent = new_root;
  }
  btree_node_insert(left_node->parent, mid_pair);
  btree_node_split(left_node->parent);
}

/*
 * if node is full, split it
 */
static void btree_node_split(struct btree_node* node) {
  if (node->size != node->tree->order * 2 + 1) {
    return;
  }

  struct btree_node* new_node;
  new_node = arena_allocate(node->tree->arena, sizeof(*new_node));
  new_node->parent = node->parent;
  new_node->size = node->tree->order;
  new_node->tree = node->tree;

  if (node->child) {
    struct btree_node* child_pos = node->child;
    for (int i = 0; i < node->tree->order; ++i) {
      child_pos = child_pos->next;
    }
    new_node->child = child_pos->next;
    child_pos->next = NULL;
    for (child_pos = new_node->child; child_pos != NULL; child_pos = child_pos->next) {
      child_pos->parent = new_node;
    }
  } else {
    new_node->child = NULL;
  }

  struct pair* pos = node->head;
  for (int i = 0; i < node->tree->order - 1; ++i) {
    pos = pos->next;
  }
  new_node->head = pos->next->next;

  struct pair* mid_pair = pos->next;
  pos->next = NULL;
  node->size = node->tree->order;
  btree_node_insert_parent(node, new_node, mid_pair);
}

static void btree_node_merge_(struct btree_node* left,
                              struct btree_node* right) {
  Assert(left && right && left->next == right);
  int order = left->tree->order;
  struct btree_node* parent = left->parent;
  Assert(parent);
  if (left->size + right->size == 2 * order - 1) {
    // borrow a pair from parent and merge it with
    // left and right node.
    if (left->child) {
      // merge left and right's child list
      struct btree_node* left_tail_child = left->child;
      while (left_tail_child->next) {
        left_tail_child = left_tail_child->next;
      }
      left_tail_child->next = right->child;

      left_tail_child = left_tail_child->next;
      while (left_tail_child) {
        left_tail_child->parent = left;
        left_tail_child = left_tail_child->next;
      }
    }

    // change parent's child list
    left->next = right->next;

    // merge node's pairs
    struct pair* left_tail = left->head;
    while (left_tail->next) {
      left_tail = left_tail->next;
    }
    if (parent->child == left) {
      left_tail->next = parent->head;
      parent->head = parent->head->next;
      left_tail->next->next = right->head;
    } else {
      struct pair* pos = parent->head;
      struct btree_node* child_pos = parent->child;
      while (child_pos->next != left) {
        pos = pos->next;
        child_pos = child_pos->next;
      }
      left_tail->next = pos->next;
      pos->next = pos->next->next;
      left_tail->next->next = right->head;
    }

    left->size = 2 * order;
    parent->size -= 1;

    if (parent->size == 0) {
      // root node empty, change root node to left
      left->tree->root = left;
      left->parent = NULL;
      return;
    }

    btree_node_merge(parent);
  } else if (left->size == order - 1) {
    struct pair* left_tail = left->head;
    while (left_tail->next) {
      left_tail = left_tail->next;
    }
    if (parent->child == left) {
      left_tail->next = parent->head;
      parent->head = right->head;
      right->head = right->head->next;
      parent->head->next = left_tail->next->next;
      left_tail->next->next = NULL;
    } else {
      struct pair* pos = parent->head;
      struct btree_node* child_pos = parent->child;
      while (child_pos->next != left) {
        pos = pos->next;
        child_pos = child_pos->next;
      }
      left_tail->next = pos->next;
      pos->next = right->head;
      right->head = right->head->next;
      pos->next->next = left_tail->next->next;
      left_tail->next->next = NULL;
    }

    if (left->child) {
      struct btree_node* left_tail_child = left->child;
      while (left_tail_child->next) {
        left_tail_child = left_tail_child->next;
      }
      left_tail_child->next = right->child;
      right->child = right->child->next;
      left_tail_child->next->next = NULL;
      left_tail_child->next->parent = left;
    }

    left->size += 1;
    right->size -= 1;
  } else {
    struct pair* left_tail_before = left->head;
    while (left_tail_before->next->next) {
      left_tail_before = left_tail_before->next;
    }
    if (parent->child == left) {
      left_tail_before->next->next = parent->head->next;
      parent->head->next = right->head;
      right->head = parent->head;
      parent->head = left_tail_before->next;
      left_tail_before->next = NULL;
    } else {
      struct pair* pos = parent->head;
      struct btree_node* child_pos = parent->child;
      while (child_pos->next != left) {
        pos = pos->next;
        child_pos = child_pos->next;
      }
      left_tail_before->next->next = pos->next->next;
      pos->next->next = right->head;
      right->head = pos->next;
      pos->next = left_tail_before->next;
      left_tail_before->next = NULL;
    }

    if (left->child) {
      struct btree_node* left_tail_before_child = left->child;
      while (left_tail_before_child->next->next) {
        left_tail_before_child = left_tail_before_child->next;
      }
      left_tail_before_child->next->next = right->child;
      right->child = left_tail_before_child->next;
      left_tail_before_child->next = NULL;
      right->child->parent = right;
    }

    left->size -= 1;
    right->size += 1;
  }
}

static void btree_node_merge(struct btree_node* node) {
  if (node->size >= node->tree->order || node->parent == NULL) {
    return;
  }
  Assert(node->size == node->tree->order - 1);
  if (node->next) {
    btree_node_merge_(node, node->next);
  } else {
    struct btree_node* sibling = node->parent->child;
    while (sibling->next != node) {
      sibling = sibling->next;
    }
    btree_node_merge_(sibling, node);
  }
}

/*
 * create btree
 *
 * number of data in btree node is between [order, 2 * order]
 */
struct btree* btree_new(int order, key_cmp_t key_cmp) {
  // if order less than 2, btree_node_merge_ maybe fail
  assert(order > 2);
  struct btree* tree = malloc(sizeof(*tree));
  tree->order = order;
  tree->size = 0;
  tree->key_cmp = key_cmp;
  tree->arena = arena_new();
  tree->root = arena_allocate(tree->arena, sizeof(*tree->root));
  tree->root->size = 0;
  tree->root->child = NULL;
  tree->root->head = NULL;
  tree->root->next = NULL;
  tree->root->parent = NULL;
  tree->root->tree = tree;
  return tree;
}

void btree_destroy(struct btree* tree) {
  arena_destroy(tree->arena);
  free(tree);
}

/*
 * find key in btree
 *
 * @leaf if find, return that node; else return the leaf node which
 *       key should insert to.
 * @value if find, return key's value
 * @return 1 if find, 0 if not
 */
static int btree_lookup_(struct btree* tree, void* key,
                         struct btree_node** leaf, void** value) {
  struct btree_node* node = tree->root;
  while (node) {
    if (node->child == NULL) {
      struct pair* pos = node->head;
      while (pos && key_less(pos->key, key)) {
        pos = pos->next;
      }
      if (pos && key_equal(pos->key, key)) {
        *leaf = node;
        *value = pos->value;
        return 1;
      } else {
        *leaf = node;
        return 0;
      }
    } else {
      struct pair* pos = node->head;
      struct btree_node* child = node->child;
      while (pos && key_less(pos->key, key)) {
        pos = pos->next;
        child = child->next;
      }
      if (pos && key_equal(pos->key, key)) {
        *leaf = node;
        *value = pos->value;
        return 1;
      }
      node = child;
    }
  }
  assert(0);
  return 0;
}

void* btree_lookup(struct btree* tree, void* key) {
  struct btree_node* leaf;
  void* value;
  if (btree_lookup_(tree, key, &leaf, &value)) {
    return value;
  } else {
    return NULL;
  }
}

int btree_insert(struct btree* tree, void* key, void* value) {
  struct btree_node* leaf;
  void* val;
  if (btree_lookup_(tree, key, &leaf, &val)) {
    // key already inside tree!
    return -1;
  } else {
    struct pair* new_pair = arena_allocate(tree->arena, sizeof(*new_pair));
    new_pair->key = key;
    new_pair->value = value;
    btree_node_insert(leaf, new_pair);
    return 0;
  }
}

int btree_remove(struct btree* tree, void* key) {
  struct btree_node* node;
  void* value;
  if (btree_lookup_(tree, key, &node, &value)) {
    if (node->child == NULL) {
      btree_node_remove(node, key);
    } else {
      struct btree_node* child = node->child;
      struct pair* pos = node->head;
      while (!key_equal(pos->key, key)) {
        pos = pos->next;
        child = child->next;
      }
      child = child->next;
      while (child->child) {
        child = child->child;
      }
      pos->key = child->head->key;
      pos->value = child->head->value;
      btree_node_remove(child, child->head->key);
    }
    return 0;
  } else {
    // key isn't in tree
    return -1;
  }
}