#include <stdio.h>
#include <stdlib.h>

#include "hashtable.h"

// Allocates memory for a hash table (including all its buckets), stores the
// callbacks that should be used to manipulate the keys and values and returns a
// pointer to it.
struct HashTable *hashtable_create(uint64_t num_buckets,
                                   HashTableHashFunction hash,
                                   HashTableKeyEqualsFunction key_equals,
                                   HashTableCopyValueFunction copy_value,
                                   HashTableDestroyFunction destroy_key,
                                   HashTableDestroyFunction destroy_value) {
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

  // Set up the callbacks for the hash table operation.
  hashtable->hash = hash;
  hashtable->key_equals = key_equals;
  hashtable->copy_value = copy_value;
  hashtable->destroy_key = destroy_key;
  hashtable->destroy_value = destroy_value;

  // Initialize the keys counter.
  hashtable->key_count = 0;

  return hashtable;
}

// Returns the bucket index in the hash table for the given key.
static uint64_t hashtable_get_bucket_index(struct HashTable *hashtable,
                                           void *key) {
  uint64_t key_hash = hashtable->hash(key);
  return key_hash % hashtable->num_buckets;
}

// Inserts the given key and value into the hash table.
//////////////////////////////////////
// If the key doesn't already exist in the hash table, the function returns
// HT_NOTFOUND and the keyand value pointers become "owned" by the hash table.
// If the key does already exist in the hash table, the function returns
// HT_FOUND, the given key pointer becomes owned by the hash table, the old key
// pointer is destroyed (!!) and the old value pointer is destroyed (!!).
int hashtable_insert(struct HashTable *hashtable, void *key, void *value) {
  // Determine the bucket index for the key.
  uint64_t bucket_index = hashtable_get_bucket_index(hashtable, key);
  struct BucketNode *previous_node = NULL;

  pthread_mutex_lock(hashtable->mutex);
  struct BucketNode *current_node = hashtable->buckets[bucket_index];

  // Look for the key in the bucket.
  while (current_node != NULL) {
    if (hashtable->key_equals(current_node->key, key)) {
      // Found it!
      // Replace the old value with the new one and free the old value.
      void *old_value = current_node->value;
      current_node->value = value;
      hashtable->destroy_value(old_value);

      // Delete the new key since the old one is already assigned and is the
      // same.
      hashtable->destroy_key(key);

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
int hashtable_get(struct HashTable *hashtable, void *key, void **value) {
  // Determine the bucket index for the key.
  uint64_t bucket_index = hashtable_get_bucket_index(hashtable, key);

  pthread_mutex_lock(hashtable->mutex);
  struct BucketNode *current_node = hashtable->buckets[bucket_index];

  // Look for the key in the bucket.
  while (current_node != NULL) {
    if (hashtable->key_equals(current_node->key, key)) {
      // Found it!
      // Get a copy of the value and "return" a pointer to it.
      *value = hashtable->copy_value(current_node->value);

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
int hashtable_take(struct HashTable *hashtable, void *key, void **value) {
  // Determine the bucket index for the key.
  uint64_t bucket_index = hashtable_get_bucket_index(hashtable, key);
  struct BucketNode *previous_node = NULL;

  pthread_mutex_lock(hashtable->mutex);
  struct BucketNode *current_node = hashtable->buckets[bucket_index];

  while (current_node != NULL) {
    // Look for the key in the bucket.
    if (hashtable->key_equals(current_node->key, key)) {
      // Found it!
      // "Return" a pointer to the actual value and "remove" it from the bucket
      // node.
      *value = current_node->value;
      current_node->value = NULL; // Just in case.

      // Destroy the pointer to the key.
      hashtable->destroy_key(current_node->key);
      current_node->key = NULL; // Just in case.

      // Remove the node from the bucket
      if (previous_node == NULL) {
        // The key-value pair is stored in the first node of the bucket.
        hashtable->buckets[bucket_index] = current_node->next;
      } else {
        // The key-value pair is not stored in the first node of the bucket.
        previous_node->next = current_node->next;
      }

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
int hashtable_remove(struct HashTable *hashtable, void *key) {
  void *removed_value = NULL;
  int ret = hashtable_take(hashtable, key, &removed_value);
  if (ret == HT_FOUND) {
    // Destroy the value since we don't need it.
    hashtable->destroy_value(removed_value);
  }
  return ret;
}

// Recursively prints a bucket node of a hash table.
static void hashtable_print_bucket_nodes(struct HashTable *hashtable,
                                         struct BucketNode *bucket_node,
                                         void (*print_key)(void *),
                                         void (*print_value)(void *)) {
  if (bucket_node == NULL) {
    printf("-> X\n");
    return;
  }

  printf("-> (");
  print_key(bucket_node->key);
  printf(":");
  print_value(bucket_node->value);
  printf(") ");

  hashtable_print_bucket_nodes(hashtable, bucket_node->next, print_key,
                               print_value);
}

// Prints the given hasht able to standard output.
void hashtable_print(struct HashTable *hashtable,
                     HashTablePrintFunction print_key,
                     HashTablePrintFunction print_value) {
  printf("=====================\n");
  for (int i = 0; i < hashtable->num_buckets; i++) {
    printf("%03d | ", i);
    hashtable_print_bucket_nodes(hashtable, hashtable->buckets[i], print_key,
                                 print_value);
  }
  printf("=====================\n");
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
    hashtable->destroy_key(node_to_destroy->key);
    hashtable->destroy_value(node_to_destroy->value);
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