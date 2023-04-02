#ifndef __BOUNDED_DATA_HASHTABLE_H__
#define __BOUNDED_DATA_HASHTABLE_H__

#include "bounded_data.h"
#include "hashtable.h"

struct HashTable *bd_hashtable_create(uint64_t num_buckets);

int bd_hashtable_insert(struct HashTable *hashtable, struct BoundedData *key,
                        struct BoundedData *value);

int bd_hashtable_get(struct HashTable *hashtable, struct BoundedData *key,
                     struct BoundedData **value);

int bd_hashtable_take(struct HashTable *hashtable, struct BoundedData *key,
                      struct BoundedData **value);

int bd_hashtable_remove(struct HashTable *hashtable, struct BoundedData *key);

void bd_hashtable_print(struct HashTable *hashtable);

void bd_hashtable_destroy(struct HashTable *hashtable);

uint64_t bd_hashtable_key_count(struct HashTable *hashtable);

#endif