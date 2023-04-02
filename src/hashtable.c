#include <stdio.h>
#include <stdlib.h>

#include "hashtable.h"

// Allocates memory for a hash table (including all its buckets, the mutex and
// the usage queue) and stores the callback for the hash function.
struct HashTable *hashtable_create(uint64_t num_buckets) {
  // Allocate memory for the hash table.
  struct HashTable *hashtable = malloc(sizeof(struct HashTable));
  if (hashtable == NULL) {
    perror("hashtable_create malloc");
    abort();
  }

  // Allocate memory for the buckets of the hash table and initialize each to
  // NULL.
  hashtable->num_buckets = num_buckets;
  hashtable->buckets = malloc(sizeof(struct BucketNode *) * num_buckets);
  if (hashtable->buckets == NULL) {
    perror("hashtable_create malloc2");
    abort();
  }
  for (int i = 0; i < num_buckets; i++) {
    hashtable->buckets[i] = NULL;
  }

  // Allocate memory for the mutex of the hash table and initialize it.
  hashtable->mutex = malloc(sizeof(pthread_mutex_t));
  if (hashtable->mutex == NULL) {
    perror("hashtable_create malloc3");
    abort();
  }
  pthread_mutex_init(hashtable->mutex, NULL);

  hashtable->usage_queue = dlink_create();
  if (hashtable->usage_queue == NULL) {
    perror("hashtable_create malloc4");
    abort();
  }

  hashtable->key_count = 0;

  return hashtable;
}

// Returns the bucket index in the hash table for the given key.
static uint64_t hashtable_get_bucket_index(struct HashTable *hashtable,
                                           struct BoundedData *key) {
  uint64_t key_hash = bounded_data_hash(key);
  return key_hash % hashtable->num_buckets;
}

static void hashtable_set_most_used(struct HashTable *hashtable,
                                    struct BucketNode *node) {
  // Sets the given bucket node as the most used of the hash table.
  hashtable_print_usage_queue(hashtable);
  struct Dlink_Node *usage_node = dlink_node_remove(node->usage_node);
  dlink_insert_first(hashtable->usage_queue, usage_node);
  hashtable_print_usage_queue(hashtable);
}

// Inserts the given key and value into the hash table.
//////////////////////////////////////
// If the key doesn't already exist in the hash table, the function returns
// HT_NOTFOUND and the keyand value pointers become "owned" by the hash table.
// If the key does already exist in the hash table, the function returns
// HT_FOUND, the given key pointer becomes owned by the hash table, the old key
// pointer is destroyed (!!) and the old value pointer is destroyed (!!).
int hashtable_insert(struct HashTable *hashtable, struct BoundedData *key,
                     struct BoundedData *value) {
  // Determine the bucket index for the key.
  uint64_t bucket_index = hashtable_get_bucket_index(hashtable, key);
  struct BucketNode *previous_node = NULL;

  pthread_mutex_lock(hashtable->mutex);
  struct BucketNode *current_node = hashtable->buckets[bucket_index];

  // Look for the key in the bucket.
  while (current_node != NULL) {
    if (bounded_data_equals(current_node->key, key)) {
      // Found it!
      // Free the old value and replace it with the new one.
      bounded_data_destroy(current_node->value);
      current_node->value = value;

      // Delete the new key since the old one is already assigned and is the
      // same.
      bounded_data_destroy(key);

      hashtable_set_most_used(hashtable, current_node);

      // Return HT_FOUND to signal that the key was found when inserting.
      pthread_mutex_unlock(hashtable->mutex);
      return HT_FOUND;
    }

    // Keep looking for the key in the bucket nodes.
    previous_node = current_node;
    current_node = current_node->next;
  }

  // Didn't find the key. Add it to the bucket.
  struct BucketNode *new_node = bucket_node_create(key, value);

  // Create and set the new node as the most recently used one.
  struct Dlink_Node *new_usage_node = dlink_node_create(new_node);
  new_node->usage_node = new_usage_node;
  dlink_insert_first(hashtable->usage_queue, new_usage_node);
  if (previous_node == NULL) {
    hashtable->buckets[bucket_index] = new_node;
  } else {
    previous_node->next = new_node;
  }

  // Increase the keys counter.
  hashtable->key_count++;

  // Return HT_NOTFOUND to signal that the key wasn't found when inserting.
  pthread_mutex_unlock(hashtable->mutex);
  return HT_NOTFOUND;
}

// Attempts to retrieve a *copy* of the value associated to the given key in the
// hash table.
//////////////////////////////////////
// If the key doesn't already exist in the hash table, the function returns
// HT_NOTFOUND and the given value pointer is left untouched. If the key does
// already exist in the hash table, the function returns HT_FOUND and the given
// value pointer is modified so that it holds a pointer to a copy of the value
// associated to the given key in the hash table. The pointer of the given key
// is owned by the client.
int hashtable_get(struct HashTable *hashtable, struct BoundedData *key,
                  struct BoundedData **value) {
  // Determine the bucket index for the key.
  uint64_t bucket_index = hashtable_get_bucket_index(hashtable, key);

  pthread_mutex_lock(hashtable->mutex);
  struct BucketNode *current_node = hashtable->buckets[bucket_index];

  // Look for the key in the bucket.
  while (current_node != NULL) {
    if (bounded_data_equals(current_node->key, key)) {
      // Found it!
      // Get a copy of the value and "return" a pointer to it.
      *value = bounded_data_duplicate(current_node->value);

      hashtable_set_most_used(hashtable, current_node);

      // Return HT_FOUND to signal that the key was found when retrieving.
      pthread_mutex_unlock(hashtable->mutex);
      return HT_FOUND;
    }

    // Keep looking for the key in the bucket nodes.
    current_node = current_node->next;
  }

  // Return HT_NOTFOUND to signal that the key wasn't found when retrieving.
  pthread_mutex_unlock(hashtable->mutex);
  return HT_NOTFOUND;
}

// Attempts to remove the given key and its associated value from the hash
// table and "returns" a pointer to a copy of the removed value.
//////////////////////////////////////
// If the key doesn't already exist in the hash table, the function returns
// HT_NOTFOUND and the given value pointer is left untouched. If the key does
// already exist in the hash table, the function returns HT_FOUND, the given
// value pointer is modified so that it holds a pointer to the value associated
// to the given key in the hash table and the key pointer in the hash table is
// destroyed (!!).
int hashtable_take(struct HashTable *hashtable, struct BoundedData *key,
                   struct BoundedData **value) {
  // Determine the bucket index for the key.
  uint64_t bucket_index = hashtable_get_bucket_index(hashtable, key);
  struct BucketNode *previous_node = NULL;

  pthread_mutex_lock(hashtable->mutex);
  struct BucketNode *current_node = hashtable->buckets[bucket_index];

  while (current_node != NULL) {
    // Look for the key in the bucket.
    if (bounded_data_equals(current_node->key, key)) {
      // Found it!
      // "Return" a pointer to the actual value and "remove" it from the bucket
      // node.
      *value = current_node->value;
      current_node->value = NULL; // Just in case.

      // Destroy the pointer to the key.
      bounded_data_destroy(current_node->key);
      current_node->key = NULL; // Just in case.

      // Remove the node from the bucket
      if (previous_node == NULL) {
        // The key-value pair is stored in the first node of the bucket.
        hashtable->buckets[bucket_index] = current_node->next;
      } else {
        // The key-value pair is not stored in the first node of the bucket.
        previous_node->next = current_node->next;
      }

      // Destroy the usage node for the current bucket node.
      dlink_node_destroy(current_node->usage_node);

      // Destroy the bucket node of the key-value pair.
      bucket_node_destroy(current_node);

      // Decrease the keys counter.
      hashtable->key_count--;

      // Return HT_FOUND to signal that the key was found when removing.
      pthread_mutex_unlock(hashtable->mutex);
      return HT_FOUND;
    }

    // Keep looking for the key in the bucket nodes.
    previous_node = current_node;
    current_node = current_node->next;
  }

  // Return HT_NOTFOUND to signal that the key wasn't found when removing.
  pthread_mutex_unlock(hashtable->mutex);
  return HT_NOTFOUND;
}

// Attempts to remove the given key and its associated value from the hash
// table and get a pointer to a copy of the removed value.
//////////////////////////////////////
// If the key doesn't already exist in the hash table, the function returns
// HT_NOTFOUND. If the key does already exist in the hash table, the function
// returns HT_FOUND, the key pointer in the hash table is destroyed (!!) and the
// value pointer in the hash table is destroyed (!!).
int hashtable_remove(struct HashTable *hashtable, struct BoundedData *key) {
  struct BoundedData *removed_value = NULL;
  int ret = hashtable_take(hashtable, key, &removed_value);
  if (ret == HT_FOUND) {
    // Destroy the value since we don't need it.
    bounded_data_destroy(removed_value);
  }
  return ret;
}

// Recursively prints a bucket node of a hash table.
static void hashtable_print_bucket_nodes(struct HashTable *hashtable,
                                         struct BucketNode *bucket_node) {
  if (bucket_node == NULL) {
    printf("-> X\n");
    return;
  }

  printf("-> (");
  bounded_data_print(bucket_node->key);
  printf(":");
  bounded_data_print(bucket_node->value);
  printf(") ");

  hashtable_print_bucket_nodes(hashtable, bucket_node->next);
}

// Prints the given hashtable to standard output.
void hashtable_print(struct HashTable *hashtable) {
  printf("=====================\n");
  for (int i = 0; i < hashtable->num_buckets; i++) {
    printf("%03d | ", i);
    hashtable_print_bucket_nodes(hashtable, hashtable->buckets[i]);
  }
  printf("=====================\n");
}

// Prints the usage queue of the given hashtable to standard output.
void hashtable_print_usage_queue(struct HashTable *hashtable) {
  printf("Least used | ");

  struct Dlink_Node *current_node = hashtable->usage_queue->last;
  while (current_node != NULL) {
    struct BucketNode *bucket_node = current_node->data;
    printf("[");
    bounded_data_print(bucket_node->key);
    printf("] -> ");

    current_node = current_node->previous;
  }

  printf("| Most used\n");
}

// De-allocates the memory for all the nodes in the bucket and the bucket
// itself. De-allocates the memory of all the keys and values in the nodes as
// well.
static void hashtable_destroy_bucket(struct HashTable *hashtable,
                                     int bucket_index) {
  // Fetch the first node of the bucket.
  struct BucketNode *current_node = hashtable->buckets[bucket_index];
  while (current_node != NULL) {
    struct BucketNode *node_to_destroy = current_node;
    current_node = current_node->next;
    bounded_data_destroy(node_to_destroy->key);
    bounded_data_destroy(node_to_destroy->value);
    dlink_node_destroy(node_to_destroy->usage_node);
    bucket_node_destroy(node_to_destroy);
  }
}

// De-allocates memory for the hash table and all its keys and values.
void hashtable_destroy(struct HashTable *hashtable) {
  for (int i = 0; i < hashtable->num_buckets; i++) {
    hashtable_destroy_bucket(hashtable, i);
  }
  free(hashtable->buckets);

  int ret = pthread_mutex_destroy(hashtable->mutex);
  if (ret != 0) {
    perror("hashtable_destroy pthread_mutex_destroy");
    abort();
  }
  free(hashtable->mutex);

  free(hashtable);
}

// Returns the number of keys stored in the hash table.
uint64_t hashtable_key_count(struct HashTable *hashtable) {
  return hashtable->key_count;
}

// devuelve HT_NOTFOUND si no puede encontrar un miembro para borrar. devuelve
// HT_NOTFOUND si pudimos expulsar un par clave valor de la tabla.
int hashtable_evict_lru(struct HashTable *hashtable) {

  pthread_mutex_lock(hashtable->mutex);

  struct Dlink_Node *dlink_node = dlink_remove_last(hashtable->usage_queue);
  if (dlink_node == NULL) {
    printf("Oh shit man we don't have any more items in the usage queue.\n");
    pthread_mutex_unlock(hashtable->mutex);
    return HT_NOTFOUND;
  }

  struct BucketNode *victim_bucket_node = dlink_node->data;
  bounded_data_destroy(victim_bucket_node->key);
  bounded_data_destroy(victim_bucket_node->value);
  if (victim_bucket_node->usage_node != dlink_node) {
    printf("Oh shit, the nodes aren't matching.\n");
  }
  dlink_node_destroy(dlink_node);
  bucket_node_destroy(victim_bucket_node);

  hashtable->key_count--;

  pthread_mutex_unlock(hashtable->mutex);
  return HT_FOUND;
}