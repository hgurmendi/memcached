#include "bucket.h"

#include <stdio.h>
#include <stdlib.h>

/* Creates a new bucket node with the given key and value (and its respective
 * sizes).
 */
struct BucketNode *node_create(uint32_t key_size, char *key,
                               uint32_t value_size, char *value) {
  struct BucketNode *node = calloc(1, sizeof(struct BucketNode));
  if (node == NULL) {
    perror("calloc node_create");
    abort();
  }

  node->key_size = key_size;
  node->key = key;
  node->value_size = value_size;
  node->value = value;

  return node;
}

/* Destroys the given bucket node, freeing the memory for the `key` and `value`
 * members (if not NULL) and frees the memory used for the bucket node itself.
 */
void node_destroy(struct BucketNode *node) {
  if (node->key != NULL) {
    free(node->key);
  }
  if (node->value != NULL) {
    free(node->value);
  }
  free(node);
}

/* Initializes a bucket.
 */
void bucket_initialize(struct Bucket *bucket) {
  bucket->node = NULL;
  pthread_mutex_init(&(bucket->lock), NULL);
}

/* Traverses and destroys all the nodes of a bucket from the hash table.
 * Assumes that the `key` and `value` member can be freed.
 */
static void bucket_destroy_nodes(struct BucketNode *first_node) {
  struct BucketNode *current_node = first_node;

  while (current_node != NULL) {
    struct BucketNode *node_to_destroy = current_node;
    current_node = current_node->next;
    node_destroy(node_to_destroy);
  }
}

/* Destroys the given bucket.
 */
void bucket_destroy(struct Bucket *bucket) {
  bucket_destroy_nodes(bucket->node);

  pthread_mutex_destroy(&(bucket->lock));
}