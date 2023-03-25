#include "bounded_data_hashtable.h"

/* Returns the 64-bit FNV-1a hash for the given data.
 * More information: https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
 */
#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL
static uint64_t bd_hash(void *_bounded_data) {
  struct BoundedData *bounded_data = _bounded_data;

  uint64_t hash = FNV_OFFSET;
  for (int i = 0; i < bounded_data->size; i++) {
    hash ^= (uint64_t)(unsigned char)bounded_data->data[i];
    hash *= FNV_PRIME;
  }
  return hash;
}

static bool bd_key_equals(void *a, void *b) {
  return bounded_data_equals((struct BoundedData *)a, (struct BoundedData *)b);
}

static void *bd_copy_value(void *bounded_data) {
  return (void *)bounded_data_duplicate((struct BoundedData *)bounded_data);
}

static void bd_destroy(void *bounded_data) {
  bounded_data_destroy((struct BoundedData *)bounded_data);
}

static void bd_print(void *bounded_data) {
  bounded_data_print((struct BoundedData *)bounded_data);
}

struct HashTable *bd_hashtable_create(uint64_t num_buckets) {
  return hashtable_create(num_buckets, bd_hash, bd_key_equals, bd_copy_value,
                          bd_destroy, bd_destroy);
}

int bd_hashtable_insert(struct HashTable *hashtable, struct BoundedData *key,
                        struct BoundedData *value) {
  return hashtable_insert(hashtable, (void *)key, (void *)value);
}

int bd_hashtable_get(struct HashTable *hashtable, struct BoundedData *key,
                     struct BoundedData **value) {
  return hashtable_get(hashtable, (void *)key, (void **)value);
}

int bd_hashtable_take(struct HashTable *hashtable, struct BoundedData *key,
                      struct BoundedData **value) {
  return hashtable_take(hashtable, (void *)key, (void **)value);
}

int bd_hashtable_remove(struct HashTable *hashtable, struct BoundedData *key) {
  return hashtable_remove(hashtable, (void *)key);
}

void bd_hashtable_print(struct HashTable *hashtable) {
  hashtable_print(hashtable, bd_print, bd_print);
}

void bd_hashtable_destroy(struct HashTable *hashtable) {
  hashtable_destroy(hashtable);
}