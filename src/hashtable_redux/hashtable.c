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

  // Allocate memory for the buckets of the hash table and create each one of
  // them.
  hashtable->num_buckets = num_buckets;
  hashtable->buckets = malloc(sizeof(struct Bucket *) * num_buckets);
  if (hashtable->buckets == NULL) {
    perror("hashtable_create malloc2");
    abort();
  }
  for (int i = 0; i < num_buckets; i++) {
    hashtable->buckets[i] = bucket_create();
  }

  // Set up the callbacks for the hash table operation.
  hashtable->hash = hash;
  hashtable->key_equals = key_equals;
  hashtable->copy_value = copy_value;
  hashtable->destroy_key = destroy_key;
  hashtable->destroy_value = destroy_value;

  return hashtable;
}

// Inserts the given key and value into the hash table.
//////////////////////////////////////
// If the key doesn't already exist in the hash table, the function returns
// HT_NOTFOUND and the keyand value pointers become "owned" by the hash table.
// If the key does already exist in the hash table, the function returns
// HT_FOUND, the given key pointer becomes owned by the hash table, the old key
// pointer is destroyed (!!) and the old value pointer is destroyed (!!).
int hashtable_insert(struct HashTable *hashtable, void *key, void *value) {
  // Determine the bucket for the key.
  uint64_t key_hash = hashtable->hash(key);
  uint64_t target_bucket = key_hash % hashtable->num_buckets;
  struct Bucket *bucket = hashtable->buckets[target_bucket];

  pthread_mutex_lock(bucket->mutex);
  struct BucketNode *previous_node = NULL;
  struct BucketNode *current_node = bucket->node;

  // Look for the key in the bucket.
  while (current_node != NULL) {
    if (hashtable->key_equals(current_node->key, key)) {
      // Found it!
      // Replace the old value with the new one and free the old value.
      void *old_value = current_node->value;
      current_node->value = value;
      hashtable->destroy_value(old_value);

      // Replace the old key with the new one and free the old key.
      void *old_key = current_node->key;
      current_node->key = key;
      hashtable->destroy_key(old_key);

      // Return HT_FOUND to signal that the key was found when inserting.
      pthread_mutex_unlock(bucket->mutex);
      return HT_FOUND;
    }

    // Keep looking for the key in the bucket nodes.
    previous_node = current_node;
    current_node = current_node->next;
  }

  // Didn't find the key. Add it to the bucket.
  struct BucketNode *new_node = node_create(key, value);
  if (previous_node == NULL) {
    bucket->node = new_node;
  } else {
    previous_node->next = new_node;
  }

  // Return HT_NOTFOUND to signal that the key wasn't found when inserting.
  pthread_mutex_unlock(bucket->mutex);
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
  // Determine the bucket for the key.
  uint64_t key_hash = hashtable->hash(key);
  uint64_t target_bucket = key_hash % hashtable->num_buckets;
  struct Bucket *bucket = hashtable->buckets[target_bucket];

  pthread_mutex_lock(bucket->mutex);
  struct BucketNode *current_node = bucket->node;

  // Look for the key in the bucket.
  while (current_node != NULL) {
    if (hashtable->key_equals(current_node->key, key)) {
      // Found it!
      // Get a copy of the value and "return" a pointer to it.
      void *copied_value = hashtable->copy_value(current_node->value);
      *value = copied_value;

      // Return HT_FOUND to signal that the key was found when retrieving.
      pthread_mutex_unlock(bucket->mutex);
      return HT_FOUND;
    }

    // Keep looking for the key in the bucket nodes.
    current_node = current_node->next;
  }

  // Return HT_NOTFOUND to signal that the key wasn't found when retrieving.
  pthread_mutex_unlock(bucket->mutex);
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
  // Determine the bucket for the key.
  uint64_t key_hash = hashtable->hash(key);
  uint64_t target_bucket = key_hash % hashtable->num_buckets;
  struct Bucket *bucket = hashtable->buckets[target_bucket];

  pthread_mutex_lock(bucket->mutex);
  struct BucketNode *previous_node = NULL;
  struct BucketNode *current_node = bucket->node;

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
        bucket->node = current_node->next;
      } else {
        // The key-value pair is not stored in the first node of the bucket.
        previous_node->next = current_node->next;
      }

      // Destroy the bucket node of the key-value pair.
      node_destroy(current_node);

      // Return HT_FOUND to signal that the key was found when removing.
      pthread_mutex_unlock(bucket->mutex);
      return HT_FOUND;
    }

    // Keep looking for the key in the bucket nodes.
    previous_node = current_node;
    current_node = current_node->next;
  }

  // Return HT_NOTFOUND to signal that the key wasn't found when removing.
  pthread_mutex_unlock(bucket->mutex);
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
  struct Bucket *bucket;
  printf("=====================\n");
  for (int i = 0; i < hashtable->num_buckets; i++) {
    bucket = hashtable->buckets[i];
    printf("%03d | ", i);
    hashtable_print_bucket_nodes(hashtable, bucket->node, print_key,
                                 print_value);
  }
  printf("=====================\n");
}

// De-allocates the memory for all the nodes in the bucket and the bucket
// itself. De-allocates the memory of all the keys and values in the nodes as
// well.
static void hashtable_destroy_bucket(struct HashTable *hashtable,
                                     int bucket_index) {
  struct Bucket *bucket = hashtable->buckets[bucket_index];
  struct BucketNode *current_node = bucket->node;
  while (current_node != NULL) {
    struct BucketNode *node_to_destroy = current_node;
    current_node = current_node->next;
    hashtable->destroy_key(node_to_destroy->key);
    hashtable->destroy_value(node_to_destroy->value);
    node_destroy(node_to_destroy);
  }
  bucket_destroy(bucket);
}

// De-allocates memory for the hash table and all its keys and values.
void hashtable_destroy(struct HashTable *hashtable) {
  for (int i = 0; i < hashtable->num_buckets; i++) {
    hashtable_destroy_bucket(hashtable, i);
  }
  free(hashtable->buckets);
  free(hashtable);
}