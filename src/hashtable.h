#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include <stdint.h>

#include "bucket.h"

// Hash function for the hash table. Returns the hash of a key of the hash
// table.
typedef uint64_t (*HashTableHashFunction)(void *);

// Key comparison function for the hash table. Returns `true` if both keys are
// considered equal, `false` otherwise.
typedef bool (*HashTableKeyEqualsFunction)(void *, void *);

// Value copy function for the hash table. Allocates memory for a copy of a
// value already present in the hash table and returns a pointer to it. It's
// responsibility of the caller to free this memory.
typedef void *(*HashTableCopyValueFunction)(void *);

// Destroy function for the hash table. Destroys the memory allocated for a key
// that is about to be removed from the hash table or for a value that is about
// to be replaced in the hash table.
typedef void (*HashTableDestroyFunction)(void *);

// Print function for the hash table. Prints a key or value to standard output.
typedef void (*HashTablePrintFunction)(void *);

#define HT_FOUND 1
#define HT_NOTFOUND 2
#define HT_ERROR 3

struct HashTable {
  uint64_t num_buckets;
  struct BucketNode **buckets;
  HashTableHashFunction hash;
  HashTableKeyEqualsFunction key_equals;
  HashTableCopyValueFunction copy_value;
  HashTableDestroyFunction destroy_key;
  HashTableDestroyFunction destroy_value;
  pthread_mutex_t *mutex;
};

// Allocates memory for a hash table (including all its buckets), stores the
// callbacks that should be used to manipulate the keys and values and returns a
// pointer to it.
struct HashTable *hashtable_create(uint64_t num_buckets,
                                   HashTableHashFunction hash,
                                   HashTableKeyEqualsFunction key_equals,
                                   HashTableCopyValueFunction copy_value,
                                   HashTableDestroyFunction destroy_key,
                                   HashTableDestroyFunction destroy_value);

// Inserts the given key and value into the hash table.
//////////////////////////////////////
// If the key doesn't already exist in the hash table, the function returns
// HT_NOTFOUND and the keyand value pointers become "owned" by the hash table.
// If the key does already exist in the hash table, the function returns
// HT_FOUND, the given key pointer becomes owned by the hash table, the old key
// pointer is destroyed (!!) and the old value pointer is destroyed (!!).
int hashtable_insert(struct HashTable *hashtable, void *key, void *value);

// Attempts to retrieve a *copy* of the value associated to the given key in the
// hash table.
//////////////////////////////////////
// If the key doesn't already exist in the hash table, the function returns
// HT_NOTFOUND and the given value pointer is left untouched. If the key does
// already exist in the hash table, the function returns HT_FOUND and the given
// value pointer is modified so that it holds a pointer to a copy of the value
// associated to the given key in the hash table. The pointer of the given key
// is owned by the client.
int hashtable_get(struct HashTable *hashtable, void *key, void **value);

// Attempts to remove the given key and its associated value from the hash
// table and "returns" a pointer to a copy of the removed value.
//////////////////////////////////////
// If the key doesn't already exist in the hash table, the function returns
// HT_NOTFOUND and the given value pointer is left untouched. If the key does
// already exist in the hash table, the function returns HT_FOUND, the given
// value pointer is modified so that it holds a pointer to the value associated
// to the given key in the hash table and the key pointer in the hash table is
// destroyed (!!).
int hashtable_take(struct HashTable *hashtable, void *key, void **value);

// Attempts to remove the given key and its associated value from the hash
// table and get a pointer to a copy of the removed value.
//////////////////////////////////////
// If the key doesn't already exist in the hash table, the function returns
// HT_NOTFOUND. If the key does already exist in the hash table, the function
// returns HT_FOUND, the key pointer in the hash table is destroyed (!!) and the
// value pointer in the hash table is destroyed (!!).
int hashtable_remove(struct HashTable *hashtable, void *key);

// Prints the given hashtable to standard output.
void hashtable_print(struct HashTable *hashtable,
                     HashTablePrintFunction print_key,
                     HashTablePrintFunction print_value);

// De-allocates memory for the hash table and all its keys and values.
void hashtable_destroy(struct HashTable *hashtable);

#endif