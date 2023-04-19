#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"
#include "parameters.h"

// Acquires the mutex of the given bucket of the hash table.
static void hashtable_bucket_acquire(struct HashTable *hashtable,
                                     uint64_t bucket_index) {
  pthread_mutex_lock(hashtable->bucket_mutexes + bucket_index);
}

// Tries to acquire the mutex of the given bucket of the hash table. Returns
// true if successfully acquires the mutex, false otherwise.
static bool hashtable_bucket_try_acquire(struct HashTable *hashtable,
                                         uint64_t bucket_index) {
  return pthread_mutex_trylock(hashtable->bucket_mutexes + bucket_index) == 0;
}

// Releases the mutex of the given bucket of the hash table.
static void hashtable_bucket_release(struct HashTable *hashtable,
                                     uint64_t bucket_index) {
  pthread_mutex_unlock(hashtable->bucket_mutexes + bucket_index);
}

// Acquires the mutex of the key count of the hash table.
static void hashtable_key_count_acquire(struct HashTable *hashtable) {
  pthread_mutex_lock(hashtable->key_count_mutex);
}

// Releases the mutex of the key count of the hash table.
static void hashtable_key_count_release(struct HashTable *hashtable) {
  pthread_mutex_unlock(hashtable->key_count_mutex);
}

// Acquires the mutex of the usage queue of the hash table.
static void hashtable_usage_acquire(struct HashTable *hashtable) {
  pthread_mutex_lock(hashtable->usage_mutex);
}

// Releases the mutex of the usage queue of the hash table.
static void hashtable_usage_release(struct HashTable *hashtable) {
  pthread_mutex_unlock(hashtable->usage_mutex);
}

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

  // Allocate memory for the mutexes of each bucket of the hash table and
  // initialize them
  hashtable->bucket_mutexes = malloc(sizeof(pthread_mutex_t) * num_buckets);
  if (hashtable->bucket_mutexes == NULL) {
    perror("hashtable_create malloc3");
    abort();
  }
  for (int i = 0; i < num_buckets; i++) {
    pthread_mutex_init(hashtable->bucket_mutexes + i, NULL);
  }

  hashtable->key_count = 0;
  hashtable->key_count_mutex = malloc(sizeof(pthread_mutex_t));
  if (hashtable->key_count_mutex == NULL) {
    perror("hashtable_create malloc4");
    abort();
  }
  pthread_mutex_init(hashtable->key_count_mutex, NULL);

  hashtable->most_used = NULL;
  hashtable->least_used = NULL;

  hashtable->usage_mutex = malloc(sizeof(pthread_mutex_t) * num_buckets);
  if (hashtable->usage_mutex == NULL) {
    perror("hashtable_create malloc5");
    abort();
  }
  pthread_mutex_init(hashtable->usage_mutex, NULL);

  return hashtable;
}

// Returns the bucket index in the hash table for the given key.
static uint64_t hashtable_get_bucket_index(struct HashTable *hashtable,
                                           struct BoundedData *key) {
  uint64_t key_hash = bounded_data_hash(key);
  return key_hash % hashtable->num_buckets;
}

// Given a usage node that is not in the usage queue, add it as the most used
// node. Assumes that the usage queue mutex is acquired.
static void
hashtable_insert_as_most_used_usage_node(struct HashTable *hashtable,
                                         struct UsageNode *usage_node) {
  usage_node->more_used = NULL;
  usage_node->less_used = hashtable->most_used;
  if (hashtable->most_used == NULL) {
    hashtable->least_used = usage_node;
  } else {
    hashtable->most_used->more_used = usage_node;
  }
  hashtable->most_used = usage_node;
}

// Given a usage node that is in the usage queue, unlink it from the queue.
// Assumes that the usage queue mutex is acquired.
static void hashtable_remove_usage_node(struct HashTable *hashtable,
                                        struct UsageNode *usage_node) {
  struct UsageNode *less = usage_node->less_used;
  struct UsageNode *more = usage_node->more_used;

  if (less != NULL) {
    less->more_used = more;
    usage_node->less_used = NULL;
  }

  if (more != NULL) {
    more->less_used = less;
    usage_node->more_used = NULL;
  }

  if (less == NULL) {
    // we removed the least used element.
    hashtable->least_used = more;
  }

  if (more == NULL) {
    // we removed the most used element.
    hashtable->most_used = less;
  }
}

// Inserts the given key and value into the hash table.
//////////////////////////////////////
// If the key doesn't already exist in the hash table, the function returns
// HT_NOTFOUND and the keyand value pointers become "owned" by the hash table.
// If the key does already exist in the hash table, the function returns
// HT_FOUND, the given key pointer becomes owned by the hash table, the old key
// pointer is destroyed (!!) and the old value pointer is destroyed (!!). If
// there is not enough memory for the new entry after evictions, both key and
// value pointers are destroyed and HT_ERROR is returned.
int hashtable_insert(struct HashTable *hashtable, struct BoundedData *key,
                     struct BoundedData *value) {
  // Determine the bucket index for the key.
  uint64_t bucket_index = hashtable_get_bucket_index(hashtable, key);
  struct BucketNode *previous_node = NULL;

  hashtable_bucket_acquire(hashtable, bucket_index);
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

      // Set as the most used.

      hashtable_usage_acquire(hashtable);
      hashtable_remove_usage_node(hashtable, current_node->usage_node);
      hashtable_insert_as_most_used_usage_node(hashtable,
                                               current_node->usage_node);
      hashtable_usage_release(hashtable);

      // Return HT_FOUND to signal that the key was found when inserting.
      hashtable_bucket_release(hashtable, bucket_index);
      return HT_FOUND;
    }

    // Keep looking for the key in the bucket nodes.
    previous_node = current_node;
    current_node = current_node->next;
  }

  // Didn't find the key. Create a bucket node and add it to the bucket.
  struct BucketNode *new_node =
      hashtable_malloc_evict(hashtable, sizeof(struct BucketNode));
  if (new_node == NULL) {
    hashtable_bucket_release(hashtable, bucket_index);
    bounded_data_destroy(key);
    bounded_data_destroy(value);
    return HT_ERROR;
  }
  new_node->key = key;
  new_node->value = value;
  new_node->next = NULL;       // Just in case.
  new_node->usage_node = NULL; // Just in case.

  // Create and set the new node as the most recently used one.
  struct UsageNode *new_usage_node =
      hashtable_malloc_evict(hashtable, sizeof(struct UsageNode));
  if (new_usage_node == NULL) {
    hashtable_bucket_release(hashtable, bucket_index);
    free(new_node);
    bounded_data_destroy(key);
    bounded_data_destroy(value);
    return HT_ERROR;
  }
  new_usage_node->less_used = NULL; // Just in case.
  new_usage_node->more_used = NULL; // Just in case.
  new_usage_node->bucket_node = new_node;
  new_node->usage_node = new_usage_node;

  hashtable_usage_acquire(hashtable);
  hashtable_insert_as_most_used_usage_node(hashtable, new_usage_node);
  hashtable_usage_release(hashtable);

  if (previous_node == NULL) {
    hashtable->buckets[bucket_index] = new_node;
  } else {
    previous_node->next = new_node;
  }

  // Return HT_NOTFOUND to signal that the key wasn't found when inserting.
  hashtable_bucket_release(hashtable, bucket_index);

  // Increase the keys counter.
  hashtable_key_count_acquire(hashtable);
  hashtable->key_count++;
  hashtable_key_count_release(hashtable);

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
// is owned by the client. If there is not enough memory for the duplicate of
// the value then HT_ERROR is returned.
int hashtable_get(struct HashTable *hashtable, struct BoundedData *key,
                  struct BoundedData **value) {
  // Determine the bucket index for the key.
  uint64_t bucket_index = hashtable_get_bucket_index(hashtable, key);

  hashtable_bucket_acquire(hashtable, bucket_index);
  struct BucketNode *current_node = hashtable->buckets[bucket_index];

  // Look for the key in the bucket.
  while (current_node != NULL) {
    if (bounded_data_equals(current_node->key, key)) {
      // Found it!
      // Get a copy of the value and "return" a pointer to it.
      struct BoundedData *copy = hashtable_malloc_evict_bounded_data(
          hashtable, current_node->value->size);
      if (copy == NULL) {
        hashtable_bucket_release(hashtable, bucket_index);
        return HT_ERROR;
      }
      memcpy(copy->data, current_node->value->data, current_node->value->size);
      *value = copy;

      // Set as the most used.
      hashtable_usage_acquire(hashtable);
      hashtable_remove_usage_node(hashtable, current_node->usage_node);
      hashtable_insert_as_most_used_usage_node(hashtable,
                                               current_node->usage_node);
      hashtable_usage_release(hashtable);

      // Return HT_FOUND to signal that the key was found when retrieving.
      hashtable_bucket_release(hashtable, bucket_index);
      return HT_FOUND;
    }

    // Keep looking for the key in the bucket nodes.
    current_node = current_node->next;
  }

  // Return HT_NOTFOUND to signal that the key wasn't found when retrieving.
  hashtable_bucket_release(hashtable, bucket_index);
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

  hashtable_bucket_acquire(hashtable, bucket_index);
  struct BucketNode *current_node = hashtable->buckets[bucket_index];

  while (current_node != NULL) {
    // Look for the key in the bucket.
    if (bounded_data_equals(current_node->key, key)) {
      // Found it!

      // Remove and destroy the usage node for the current bucket node.
      hashtable_usage_acquire(hashtable);
      hashtable_remove_usage_node(hashtable, current_node->usage_node);
      hashtable_usage_release(hashtable);
      free(current_node->usage_node);

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

      // Destroy the bucket node of the key-value pair.
      free(current_node);

      hashtable_bucket_release(hashtable, bucket_index);

      // Decrease the keys counter.
      hashtable_key_count_acquire(hashtable);
      hashtable->key_count--;
      hashtable_key_count_release(hashtable);

      // Return HT_FOUND to signal that the key was found when removing.
      return HT_FOUND;
    }

    // Keep looking for the key in the bucket nodes.
    previous_node = current_node;
    current_node = current_node->next;
  }

  // Return HT_NOTFOUND to signal that the key wasn't found when removing.
  hashtable_bucket_release(hashtable, bucket_index);
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
  printf("vsize%ld", bucket_node->value->size);
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
  printf("=== Key count: %ld\n", hashtable->key_count);
  printf("=====================\n");
}

// Prints the usage queue of the given hashtable to standard output.
void hashtable_print_usage_queue(struct HashTable *hashtable) {
  printf("Least used | ");

  struct UsageNode *current = hashtable->least_used;
  while (current != NULL) {
    printf("[");
    bounded_data_print(current->bucket_node->key);
    printf("] ");
    current = current->more_used;
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
    free(node_to_destroy->usage_node);
    free(node_to_destroy);
  }

  int rv = pthread_mutex_destroy(hashtable->bucket_mutexes + bucket_index);
  if (rv != 0) {
    perror("hashtable_destroy_bucket pthread_mutex_destroy");
    abort();
  }
}

// De-allocates memory for the hash table and all its keys and values.
void hashtable_destroy(struct HashTable *hashtable) {
  int rv;

  for (int i = 0; i < hashtable->num_buckets; i++) {
    // The usage nodes are destroyed when destroying the bucket.
    hashtable_destroy_bucket(hashtable, i);
  }
  free(hashtable->buckets);
  free(hashtable->bucket_mutexes);

  rv = pthread_mutex_destroy(hashtable->key_count_mutex);
  if (rv != 0) {
    perror("hashtable_destroy pthread_mutex_destroy");
    abort();
  }
  free(hashtable->key_count_mutex);

  rv = pthread_mutex_destroy(hashtable->usage_mutex);
  if (rv != 0) {
    perror("hashtable_destroy pthread_mutex_destroy2");
    abort();
  }
  free(hashtable->usage_mutex);

  free(hashtable);
}

// Returns the number of keys stored in the hash table.
uint64_t hashtable_key_count(struct HashTable *hashtable) {
  return hashtable->key_count;
}

// Evicts the entries from the hash table using a best-effort least recently
// used order: it starts trying with the least recently used entry and when
// unsuccessful it continues with the next least recently used entry and so on.
// If an eviction is successful then HT_FOUND is returned. If all eviction
// attempts were consumed without a successful eviction then HT_NOTFOUND is
// returned.
static int evict_lru(struct HashTable *hashtable) {
  int remaining_tries = MAX_EVICTION_ATTEMPTS;

  hashtable_usage_acquire(hashtable);

  struct UsageNode *victim_usage_node = hashtable->least_used;

  while (victim_usage_node != NULL && remaining_tries > 0) {
    uint64_t bucket_index = hashtable_get_bucket_index(
        hashtable, victim_usage_node->bucket_node->key);

    if (hashtable_bucket_try_acquire(hashtable, bucket_index)) {
      // Lock acquisition successful: unlink the victim bucket node from the
      // hash table's bucket linked to the victim.
      struct BucketNode *victim_bucket_node = victim_usage_node->bucket_node;
      struct BucketNode *current_bucket_node = hashtable->buckets[bucket_index];

      // current_bucket_node *shouldn't* be NULL, so we add a log just in case
      // because something very wrong is happening in that case.
      if (current_bucket_node == NULL) {
        printf(
            "CRITICAL ERROR: trying to evict an entry from an empty bucket\n");
        hashtable_bucket_release(hashtable, bucket_index);
        hashtable_usage_release(hashtable);
        return HT_NOTFOUND;
      }

      if (current_bucket_node == victim_bucket_node) {
        // The victim is the first link in the bucket.
        hashtable->buckets[bucket_index] = victim_bucket_node->next;
      } else {
        // The victim is not the first link in the bucket, find its previous
        // node.
        while (current_bucket_node->next != victim_bucket_node) {
          current_bucket_node = current_bucket_node->next;
        }
        // Now current_bucket_node holds the previous node, so we can unlink the
        // victim node.
        current_bucket_node->next = victim_bucket_node->next;
      }

      // We're done working with the bucket, so we can release the lock.
      hashtable_bucket_release(hashtable, bucket_index);

      // Remove the least used node from the usage queue.
      hashtable_remove_usage_node(hashtable, victim_usage_node);

      // We're done working with the usage queue, so we can release the lock.
      hashtable_usage_release(hashtable);

      // Free the memory that is no longer used: the victim bucket node's key
      // and value, the victim bucket node and the least used usage queue node.
      bounded_data_destroy(victim_bucket_node->key);
      bounded_data_destroy(victim_bucket_node->value);
      free(victim_bucket_node);
      free(victim_usage_node);

      // Lastly, decrease the number of keys because we just removed an element!
      hashtable_key_count_acquire(hashtable);
      hashtable->key_count--;
      hashtable_key_count_release(hashtable);

      // Return HT_FOUND to signal that a victim was successfully found and
      // evicted.
      return HT_FOUND;
    }

    // The bucket node for the current victim usage node is acquired so try with
    // the next least used node in the usage queue.
    victim_usage_node = victim_usage_node->more_used;

    // Update the number of eviction attempts.
    remaining_tries--;
  }

  // At this point we either ran out of eviction attempts or we consumed all the
  // usage queue.
  // We can release the usage queue mutex.
  hashtable_usage_release(hashtable);

  // Just in case, we log a message when we run out of nodes in the usage queue.
  if (victim_usage_node == NULL) {
    printf("CRITICAL ERROR: ran out of usage nodes to evict!\n");
  }

  return HT_NOTFOUND;
}

// Performs hashtable evictions until the maximum evictions per operation is
// reached or until the memory is successfully allocated. Returns a pointer to
// the allocated space if successful or NULL if it wasn't possible to allocate
// memory.
void *hashtable_malloc_evict(struct HashTable *hashtable, size_t size) {
  int remaining_evictions = MAX_EVICTIONS_PER_OPERATION;
  void *ptr = NULL;
  int rv;

  do {
    ptr = malloc(size);
    if (ptr == NULL) {
      rv = evict_lru(hashtable);
      if (rv == HT_NOTFOUND) {
        printf("CRITICAL ERROR (hashtable_malloc_evict): couldn't successfully "
               "evict a hash table entry\n");
        return NULL;
      }
      // Keep trying
      remaining_evictions--;
      continue;
    }
    // Success!
    return ptr;
  } while (remaining_evictions > 0);

  printf("CRITICAL ERROR: Eviction failure! All evictions per operation "
         "depleted\n");
  return NULL;
}

// Tries to allocate memory for a BoundedData struct and a buffer of the given
// size. If it fails return NULL, otherwise return a pointer to the BoundedData
// struct. If buffer_size is 0, the bounded data struct will be uninitialized.
struct BoundedData *
hashtable_malloc_evict_bounded_data(struct HashTable *hashtable,
                                    size_t buffer_size) {
  struct BoundedData *bounded_data =
      hashtable_malloc_evict(hashtable, sizeof(struct BoundedData));

  if (bounded_data == NULL) {
    return NULL;
  }

  if (buffer_size == 0) {
    return bounded_data;
  }

  bounded_data->size = buffer_size;
  bounded_data->data = hashtable_malloc_evict(hashtable, buffer_size);
  if (bounded_data->data == NULL) {
    free(bounded_data);
    return NULL;
  }

  return bounded_data;
}