#ifndef __HASH_H__
#define __HASH_H__

#include <stdint.h>

typedef uint64_t (*HashFunction)(uint32_t, char *);

uint64_t dummy_hash(uint32_t data_size, char *data);

uint64_t ordinary_hash(uint32_t data_size, char *data);

uint64_t fnv_1a_hash(uint32_t data_size, char *data);

uint64_t djb2_hash(uint32_t data_size, char *data);

uint64_t sdbm_hash(uint32_t data_size, char *data);

#endif