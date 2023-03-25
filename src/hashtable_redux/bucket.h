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

struct Bucket {
  struct BucketNode *node;
  pthread_mutex_t *mutex;
};

// Allocates memory for a bucket node, stores the given key and value in it and
// returns a pointer to the bucket node struct.
struct BucketNode *node_create(void *key, void *value);

// Frees the memory allocated for a bucket node. The key and value stored in the
// node should have been freed already.
void node_destroy(struct BucketNode *node);

// Allocates memory for an empty bucket, initializes the mutex and returns a
// pointer to the bucket struct.
struct Bucket *bucket_create();

// De-allocates memory for the given bucket, which should be (i.e. no nodes).
void bucket_destroy(struct Bucket *bucket);

#endif