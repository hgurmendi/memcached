#ifndef __BUCKET_H__
#define __BUCKET_H__

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "dlist.h"

struct BucketNode {
  struct BucketNode *next;
  uint32_t key_size;
  char *key;
  uint32_t value_size;
  char *value;

  // Node used in the LRU queue.
  struct DListNode lru_queue_node;
};

struct Bucket {
  struct BucketNode *node;
  pthread_mutex_t lock;
  uint64_t key_count;
};

/* Creates a bucket node with the given key and value (and its respective
 * sizes).
 */
struct BucketNode *node_create(uint32_t key_size, char *key,
                               uint32_t value_size, char *value);

/* Destroys the given bucket node, freeing the memory for the `key` and `value`
 * members (if not NULL) and frees the memory used for the bucket node itself.
 */
void node_destroy(struct BucketNode *node);

/* Creates a bucket.
 */
void bucket_initialize(struct Bucket *bucket);

/* Destroys the given bucket.
 */
void bucket_destroy(struct Bucket *bucket);

#endif