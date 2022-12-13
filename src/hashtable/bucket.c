#include "bucket.h"

#include <stdio.h>
#include <stdlib.h>

#include "../wrapped_free.h"

/* Creates a new bucket node with the given key and value (and its respective
 * sizes).
 */
struct BucketNode *node_create(uint32_t key_size, char *key,
                               uint32_t value_size, char *value) {
  struct BucketNode *node = malloc(sizeof(struct BucketNode));
  if (node == NULL) {
    perror("malloc node_create");
    abort();
  }

  node->next = NULL;
  node->key_size = key_size;
  node->key = key;
  node->value_size = value_size;
  node->value = value;

  // Initialize the LRU queue node with a pointer to this bucket node.
  dlist_node_initialize(&(node->lru_queue_node), (void *)node);

  return node;
}

/* Destroys the given bucket node, freeing the memory for the `key` and `value`
 * members (if not NULL) and frees the memory used for the bucket node itself.
 * Assumes that the `lru_queue_node` member is not being referenced by anyone
 * when calling this function.
 */
void node_destroy(struct BucketNode *node) {
  if (node->key != NULL) {
    wrapped_free(node->key, node->key_size);
  }
  if (node->value != NULL) {
    wrapped_free(node->value, node->value_size);
  }
  wrapped_free(node, sizeof(struct BucketNode *));
}

/* Initializes a bucket.
 */
void bucket_initialize(struct Bucket *bucket) {
  bucket->node = NULL;
  pthread_mutex_init(&(bucket->lock), NULL);
  bucket->key_count = 0;
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