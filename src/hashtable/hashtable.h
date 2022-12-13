#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include <stdint.h>

#include "bucket.h"
#include "hash.h"
#include "queue.h"

#define HT_FOUND 1
#define HT_NOTFOUND 2
#define HT_ERROR 3

struct HashTable {
  int buckets_size;
  struct Bucket *buckets;
  HashFunction hash;

  pthread_mutex_t lru_queue_lock;
  struct Queue *lru_queue;
};

/* Returns the number of unique keys present in the hash table.
 */
uint64_t hashtable_key_count(struct HashTable *hashtable);

/* Allocates all the resources necessary for a hash table with `size` buckets.
 */
struct HashTable *hashtable_create(uint32_t buckets_size, HashFunction hash);

/* Inserts a key-value pair to the hash table.
 * Returns HT_NOTFOUND if the key didn't exist already in the hash table and
 * stores the key-value pair in the hash table.
 * Returns HT_FOUND if the key already exists in the hash table, in which case
 * the value is completely replaced. The old value and the old key are freed.
 */
int hashtable_insert(struct HashTable *hashtable, uint32_t key_size, char *key,
                     uint32_t value_size, char *value);

/* Removes the key-value pair from the hash table for the given key.
 * Returns HT_NOTFOUND if the key is not found in the hash table.
 * Returns HT_FOUND if the key is found in the hash table and the key-value pair
 * was removed from the hash table. The key and the value in the hash table are
 * freed.
 */
int hashtable_remove(struct HashTable *hashtable, uint32_t key_size, char *key);

/* Retrieves the value stored for the given key in the hash table and returns
 * it.
 * Returns HT_NOTFOUND if the key is not found in the hash table, and both
 * `ret_value` and `ret_value_size` are left untouched.
 * Returns HT_FOUND if the key is found and the value and its size are written
 * to `ret_value` and `ret_value_size`, respectively.
 */
int hashtable_get(struct HashTable *hashtable, uint32_t key_size, char *key,
                  uint32_t *ret_value_size, char **ret_value);

/* Removes the key-value pair from the hash table for the given key, and returns
 * the direction for the value and its size in `ret_value` and `ret_value_size`
 * respectively. This operation is similar to a pop.
 * Returns HT_NOTFOUND if the key is not found in the hash table, and both
 * `ret_value` and `ret_value_size` are left untouched.
 * Returns HT_FOUND if the key is found and the value and its size are written
 * to `ret_value` and `ret_value_size`, respectively. It's responsibility of the
 * consumer to free the memory of the value. The memory of the key is freed.
 */
int hashtable_take(struct HashTable *hashtable, uint32_t key_size, char *key,
                   uint32_t *ret_value_size, char **ret_value);

/* Evits a key-value pair from the HashTable. It attempts from the least
 * recently used key-value pair until we can evict one.
 * Returns true if a key-value pair is successfully evicted from the hash table,
 * false otherwise.
 */
bool hashtable_evict(struct HashTable *hashtable);

/* Tries to a*/
void *hashtable_attempt_malloc(struct HashTable *hashtable, size_t size);

/* Frees up the memory used by the given hash table.
 * Frees the memory of all the keys and values.
 */
void hashtable_destroy(struct HashTable *hashtable);

/* Prints the buckets of the given hash table to stdout.
 * Assumes that the keys and values are strings.
 */
void hashtable_print(struct HashTable *hashtable);

#endif