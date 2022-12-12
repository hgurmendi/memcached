#include "hash.h"

#define FNV_OFFSET 14695981039346656037UL
#define FNV_PRIME 1099511628211UL

/* Very dummy and predictable hash function that just returns the size of the
 * data...
 */
uint64_t dummy_hash(uint32_t data_size, char *data) { return data_size; }

/* An ordinary hash function that uses sums and multiplication on all the bytes
 * of the given char string and returns the resulting 64 bit number.
 */
uint64_t ordinary_hash(uint32_t data_size, char *data) {
  uint64_t hash = 420959293004244;
  for (int i = 0; i < data_size; i++) {
    hash += data[i];
    hash *= 65599590039;
  }
  return hash;
}

/* Returns the 64-bit FNV-1a hash for the given data.
 * More information: https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
 */
uint64_t fnv_1a_hash(uint32_t data_size, char *data) {
  uint64_t hash = FNV_OFFSET;
  for (int i = 0; i < data_size; i++) {
    hash ^= (uint64_t)(unsigned char)data[i];
    hash *= FNV_PRIME;
  }
  return hash;
}

/* djb2 hash algorithm.
 * More information: http://www.cse.yorku.ca/~oz/hash.html#djb2
 */
uint64_t djb2_hash(uint32_t data_size, char *data) {
  uint64_t hash = 5381;

  for (int i = 0; i < data_size; i++) {
    hash = ((hash << 5) + hash) + data[i]; /* hash * 33 + c */
  }

  return hash;
}

uint64_t sdbm_hash(uint32_t data_size, char *data) {
  unsigned long hash = 0;

  for (int i = 0; i < data_size; i++) {
    hash = data[i] + (hash << 6) + (hash << 16) - hash;
  }

  return hash;
}