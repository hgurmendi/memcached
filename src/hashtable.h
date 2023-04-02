#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include <pthread.h>
#include <stdint.h>

#include "bounded_data.h"

#define HT_FOUND 1
#define HT_NOTFOUND 2
#define HT_ERROR 3

struct BucketNode {
  struct BucketNode *next;
  struct BoundedData *key;
  struct BoundedData *value;
  struct UsageNode *usage_node;
};

struct UsageNode {
  struct BucketNode *bucket_node;
  struct UsageNode *more_used;
  struct UsageNode *less_used;
};

struct HashTable {
  uint64_t num_buckets;
  struct BucketNode **buckets;
  pthread_mutex_t *mutex;
  uint64_t key_count;
  struct UsageNode *most_used;
  struct UsageNode *least_used;
};

// Allocates memory for a hash table (including all its buckets, the mutex and
// the usage queue) and stores the callback for the hash function.
struct HashTable *hashtable_create(uint64_t num_buckets);

// Inserts the given key and value into the hash table.
//////////////////////////////////////
// If the key doesn't already exist in the hash table, the function returns
// HT_NOTFOUND and the keyand value pointers become "owned" by the hash table.
// If the key does already exist in the hash table, the function returns
// HT_FOUND, the given key pointer becomes owned by the hash table, the old key
// pointer is destroyed (!!) and the old value pointer is destroyed (!!).
int hashtable_insert(struct HashTable *hashtable, struct BoundedData *key,
                     struct BoundedData *value);

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
                  struct BoundedData **value);

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
                   struct BoundedData **value);

// Attempts to remove the given key and its associated value from the hash
// table and get a pointer to a copy of the removed value.
//////////////////////////////////////
// If the key doesn't already exist in the hash table, the function returns
// HT_NOTFOUND. If the key does already exist in the hash table, the function
// returns HT_FOUND, the key pointer in the hash table is destroyed (!!) and the
// value pointer in the hash table is destroyed (!!).
int hashtable_remove(struct HashTable *hashtable, struct BoundedData *key);

// Prints the given hashtable to standard output.
void hashtable_print(struct HashTable *hashtable);

// Prints the usage queue of the given hashtable to standard output.
void hashtable_print_usage_queue(struct HashTable *hashtable);

// De-allocates memory for the hash table and all its keys and values.
void hashtable_destroy(struct HashTable *hashtable);

// Returns the number of keys stored in the hash table.
uint64_t hashtable_key_count(struct HashTable *hashtable);

// Evicts the least recently used entry from the hashtable. If there are no
// entries in the cache then we return HT_NOTFOUND. Otherwise, we evict the
// least recently used one and return HT_FOUND.
int hashtable_evict_lru(struct HashTable *hashtable);

#endif