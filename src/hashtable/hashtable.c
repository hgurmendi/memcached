#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"

/* Returns the number of unique keys present in the hash table.
 */
uint64_t hashtable_key_count(struct HashTable *hashtable) {
  uint64_t count = 0;

  for (int i = 0; i < hashtable->buckets_size; i++) {
    count += hashtable->buckets[i].key_count;
  }

  return count;
}

/* Allocates all the resources necessary for a hash table with `size`
 * buckets.
 */
struct HashTable *hashtable_create(uint32_t buckets_size, HashFunction hash) {
  struct HashTable *hashtable = calloc(1, sizeof(struct HashTable));
  if (hashtable == NULL) {
    perror("hashtable_create calloc hashtable");
    abort();
  }

  hashtable->hash = hash;
  hashtable->buckets_size = buckets_size;
  hashtable->buckets = calloc(buckets_size, sizeof(struct Bucket));
  if (hashtable->buckets == NULL) {
    perror("hashtable_create calloc buckets");
    abort();
  }

  for (int i = 0; i < buckets_size; i++) {
    bucket_initialize(&(hashtable->buckets[i]));
  }

  return hashtable;
}

/* True if sizes of the keys and the keys are equal byte-to-byte.
 */
static bool keys_equal(uint32_t key_size_a, char *key_a, uint32_t key_size_b,
                       char *key_b) {
  if (key_size_a != key_size_b) {
    return false;
  }

  return 0 == memcmp((void *)key_a, (void *)key_b, key_size_a);
}

/* Returns a pointer to the hash table bucket that corresponds to the given
 * key.
 */
static struct Bucket *hashtable_get_bucket_for_key(struct HashTable *hashtable,
                                                   uint32_t key_size,
                                                   char *key) {
  uint64_t key_hash = hashtable->hash(key_size, key);
  int bucket_index = key_hash % hashtable->buckets_size;
  return &(hashtable->buckets[bucket_index]);
}

/* Inserts a key-value pair to the hash table.
 * Returns HT_NOTFOUND if the key didn't exist already in the hash table and
 * stores the key-value pair in the hash table.
 * Returns HT_FOUND if the key already exists in the hash table, in which case
 * the value is completely replaced. The old value and the old key are freed.
 */
int hashtable_insert(struct HashTable *hashtable, uint32_t key_size, char *key,
                     uint32_t value_size, char *value) {
  struct Bucket *bucket =
      hashtable_get_bucket_for_key(hashtable, key_size, key);

  pthread_mutex_lock(&(bucket->lock));

  struct BucketNode *current_node = bucket->node;

  // No entries in the bucket, just add it.
  if (current_node == NULL) {
    bucket->node = node_create(key_size, key, value_size, value);

    bucket->key_count += 1;
    pthread_mutex_unlock(&(bucket->lock));
    return HT_NOTFOUND;
  }

  struct BucketNode *last_node = NULL;

  // There are entries in the bucket, replace the value if the key already
  // exists or add it to the end.
  while (current_node != NULL) {
    // The key exists in the bucket, just replace it.
    if (keys_equal(key_size, key, current_node->key_size, current_node->key)) {
      free(current_node->key);
      free(current_node->value);

      current_node->key_size = key_size;
      current_node->key = key;
      current_node->value_size = value_size;
      current_node->value = value;

      pthread_mutex_unlock(&(bucket->lock));
      return HT_FOUND;
    }

    last_node = current_node;
    current_node = current_node->next;
  }

  // The key doesn't exist in the bucket, add it after the last node.
  last_node->next = node_create(key_size, key, value_size, value);

  bucket->key_count += 1;
  pthread_mutex_unlock(&(bucket->lock));
  return HT_NOTFOUND;
}

/* Removes the key-value pair from the hash table for the given key.
 * Returns HT_NOTFOUND if the key is not found in the hash table.
 * Returns HT_FOUND if the key is found in the hash table and the key-value pair
 * was removed from the hash table. The key and the value in the hash table are
 * freed.
 */
int hashtable_remove(struct HashTable *hashtable, uint32_t key_size,
                     char *key) {
  struct Bucket *bucket =
      hashtable_get_bucket_for_key(hashtable, key_size, key);

  pthread_mutex_lock(&(bucket->lock));

  struct BucketNode *current_node = bucket->node;

  // No entries in the bucket, inform that it doesn't exist.
  if (current_node == NULL) {
    pthread_mutex_unlock(&(bucket->lock));
    return HT_NOTFOUND;
  }

  struct BucketNode *previous_node = NULL;
  // There are entries in the bucket, traverse the bucket until it's found or
  // inform that it doesn't exist.
  while (current_node != NULL) {
    // Found the key. Remove it.
    if (keys_equal(key_size, key, current_node->key_size, current_node->key)) {
      if (previous_node == NULL) {
        bucket->node = current_node->next;
      } else {
        previous_node->next = current_node->next;
      }
      node_destroy(current_node);

      bucket->key_count -= 1;
      pthread_mutex_unlock(&(bucket->lock));
      return HT_FOUND;
    }

    // Keep looking.
    previous_node = current_node;
    current_node = current_node->next;
  }

  pthread_mutex_unlock(&(bucket->lock));
  return HT_NOTFOUND;
}

/* Retrieves the value stored for the given key in the hash table and returns
 * it.
 * Returns HT_NOTFOUND if the key is not found in the hash table, and both
 * `ret_value` and `ret_value_size` are left untouched.
 * Returns HT_FOUND if the key is found in the hash table and a pointer to a
 * copy of the value and its size are written to `ret_value` and
 * `ret_value_size`, respectively. It's responsibility of the consumer to free
 * the memory of the copy of the value once they're done using it.
 * Returns HT_ERROR if memory can't be allocated for a copy of the value
 * corresponding to the key. In this case `ret_value` and `ret_value_size` are
 * left untouched.
 */
int hashtable_get(struct HashTable *hashtable, uint32_t key_size, char *key,
                  uint32_t *ret_value_size, char **ret_value) {
  struct Bucket *bucket =
      hashtable_get_bucket_for_key(hashtable, key_size, key);

  pthread_mutex_lock(&(bucket->lock));

  struct BucketNode *current_node = bucket->node;

  // No entries in the bucket, inform that it doesn't exist.
  if (current_node == NULL) {
    pthread_mutex_unlock(&(bucket->lock));
    return HT_NOTFOUND;
  }

  // There are entries in the bucket, traverse the bucket until it's found or
  // inform that it doesn't exist.
  while (current_node != NULL) {
    // The key exists in the bucket, return the associated value if we can
    // allocate enough memory for a copy of it.
    if (keys_equal(key_size, key, current_node->key_size, current_node->key)) {
      char *value_copy = (char *)malloc(current_node->value_size);
      if (value_copy == NULL) {
        pthread_mutex_unlock(&(bucket->lock));
        return HT_ERROR;
      }

      *ret_value_size = current_node->value_size;
      // Note: `memcpy` returns a pointer to the destination.
      *ret_value = memcpy((void *)value_copy, (void *)current_node->value,
                          current_node->value_size);

      pthread_mutex_unlock(&(bucket->lock));
      return HT_FOUND;
    }

    current_node = current_node->next;
  }

  pthread_mutex_unlock(&(bucket->lock));
  return HT_NOTFOUND;
}

/* Removes the key-value pair from the hash table for the given key, and returns
 * the direction for the value and its size in `ret_value` and `ret_value_size`
 * respectively. This operation is similar to a pop.
 * Returns HT_NOTFOUND if the key is not found in the hash table, and both
 * `ret_value` and `ret_value_size` are left untouched.
 * Returns HT_FOUND if the key is found and the value and its size are written
 * to `ret_value` and `ret_value_size`, respectively. It's responsibility of the
 * consumer to free the memory of the value once they're done using it. The
 * memory of the key is freed.
 */
int hashtable_take(struct HashTable *hashtable, uint32_t key_size, char *key,
                   uint32_t *ret_value_size, char **ret_value) {
  struct Bucket *bucket =
      hashtable_get_bucket_for_key(hashtable, key_size, key);

  pthread_mutex_lock(&(bucket->lock));

  struct BucketNode *current_node = bucket->node;

  // No entries in the bucket, inform that it doesn't exist.
  if (current_node == NULL) {
    pthread_mutex_unlock(&(bucket->lock));
    return HT_NOTFOUND;
  }

  struct BucketNode *previous_node = NULL;
  // There are entries in the bucket, traverse the bucket until it's found or
  // inform that it doesn't exist.
  while (current_node != NULL) {
    // Found the key. Remove it and return the value but make sure we're not
    // freeing it.
    if (keys_equal(key_size, key, current_node->key_size, current_node->key)) {
      if (previous_node == NULL) {
        bucket->node = current_node->next;
      } else {
        previous_node->next = current_node->next;
      }
      *ret_value_size = current_node->value_size;
      *ret_value = current_node->value;
      // Signal that the memory with the value shouldn't be freed, it's now
      // responsibility of the consumer.
      current_node->value = NULL;
      node_destroy(current_node);

      bucket->key_count -= 1;
      pthread_mutex_unlock(&(bucket->lock));
      return HT_FOUND;
    }

    // Keep looking.
    previous_node = current_node;
    current_node = current_node->next;
  }

  pthread_mutex_unlock(&(bucket->lock));
  return HT_NOTFOUND;
}

/* Frees up the memory used by the given hash table.
 * Frees the memory of all the keys and values.
 */
void hashtable_destroy(struct HashTable *hashtable) {
  for (int i = 0; i < hashtable->buckets_size; i++) {
    bucket_destroy(&(hashtable->buckets[i]));
  }

  free(hashtable->buckets);
  free(hashtable);
}

/* Prints a bucket to stdout.
 * Assumes that the keys and values are strings.
 */
static void hashtable_print_bucket_nodes(struct BucketNode *bucket_node) {
  if (bucket_node == NULL) {
    printf("-> X\n");
    return;
  }

  printf("-> (%s:%s) ", bucket_node->key, bucket_node->value);
  hashtable_print_bucket_nodes(bucket_node->next);
}

/* Prints the buckets of the given hash table to stdout.
 * Assumes that the keys and values are strings.
 */
void hashtable_print(struct HashTable *hashtable) {
  struct Bucket *bucket;
  printf("=====================\n");
  for (int i = 0; i < hashtable->buckets_size; i++) {
    bucket = &(hashtable->buckets[i]);
    printf("%04d [%03ld] | ", i, bucket->key_count);
    hashtable_print_bucket_nodes(bucket->node);
  }
  printf("=====================\n");
}