#include <stdlib.h>
#include <assert.h>
#include "kvengine/index.h"

int main(void) {
  struct index* index;

  index = index_new();

  for (int i = 0; i < 10000; ++i) {
    int* val;
    val = malloc(sizeof(*val));
    *val = i;
    index_insert(index, i, val);
  }

  for (int i = 0; i < 10000; ++i) {
    int* val = index_lookup(index, i);
    assert(val && *val == i);
  }

  for (int i = 0; i < 10000; ++i) {
    index_remove(index, i);
  }

  for (int i = 0; i < 10000; ++i) {
    int* val = index_lookup(index, i);
    assert(val == NULL);
  }

  index_destroy(index);

  return 0;
}