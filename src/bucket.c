#include "bucket.h"

#include <stdio.h>
#include <stdlib.h>

// Allocates memory for a bucket node, stores the given key and value in it and
// returns a pointer to the bucket node struct.
struct BucketNode *bucket_node_create(void *key, void *value) {
  // Allocate memory for the bucket node.
  struct BucketNode *node = malloc(sizeof(struct BucketNode));
  if (node == NULL) {
    perror("bucket_node_create malloc");
    abort();
  }

  node->key = key;
  node->value = value;
  node->next = NULL;

  return node;
}

// Frees the memory allocated for a bucket node. The key and value stored in the
// node should have been freed already.
void bucket_node_destroy(struct BucketNode *node) {
  // We assume that the key and value members were already freed.
  free(node);
}
