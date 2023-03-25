#include "bucket.h"

#include <stdio.h>
#include <stdlib.h>

// Allocates memory for a bucket node, stores the given key and value in it and
// returns a pointer to the bucket node struct.
struct BucketNode *node_create(void *key, void *value) {
  // Allocate memory for the bucket node.
  struct BucketNode *node = malloc(sizeof(struct BucketNode));
  if (node == NULL) {
    perror("node_create");
    abort();
  }

  node->key = key;
  node->value = value;
  node->next = NULL;

  return node;
}

// Frees the memory allocated for a bucket node. The key and value stored in the
// node should have been freed already.
void node_destroy(struct BucketNode *node) {
  // We assume that the key and value members were already freed.
  free(node);
}

// Allocates memory for an empty bucket, initializes the mutex and returns a
// pointer to the bucket struct.
struct Bucket *bucket_create() {
  // Allocate memory for the bucket.
  struct Bucket *bucket = malloc(sizeof(struct Bucket));
  if (bucket == NULL) {
    perror("bucket_create malloc");
    abort();
  }

  // The bucket starts empty, i.e. without nodes.
  bucket->node = NULL;

  // Allocate memory for the mutex.
  bucket->mutex = NULL;
  bucket->mutex = malloc(sizeof(pthread_mutex_t));
  if (bucket->mutex == NULL) {
    perror("bucket_create malloc 2");
    abort();
  }

  // Initialize the mutex.
  int err = pthread_mutex_init(bucket->mutex, NULL);
  if (err != 0) {
    perror("bucket_create pthread_mutex_init");
    abort();
  }

  return bucket;
}

// De-allocates memory for the given bucket, which should be (i.e. no nodes).
void bucket_destroy(struct Bucket *bucket) {
  // We assume that the nodes in the bucket were already freed.
  int ret = pthread_mutex_destroy(bucket->mutex);
  if (ret != 0) {
    perror("bucket_destroy pthread_mutex_destroy");
    abort();
  }
  free(bucket->mutex);
  free(bucket);
}