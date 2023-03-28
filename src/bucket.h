#ifndef __BUCKET_H__
#define __BUCKET_H__

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

struct BucketNode {
  struct BucketNode *next;
  void *key;
  void *value;
};

// Allocates memory for a bucket node, stores the given key and value in it and
// returns a pointer to the bucket node struct.
struct BucketNode *bucket_node_create(void *key, void *value);

// Frees the memory allocated for a bucket node. The key and value stored in the
// node should have been freed already.
void bucket_node_destroy(struct BucketNode *node);

#endif