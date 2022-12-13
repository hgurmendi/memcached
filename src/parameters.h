#ifndef __PARAMETERS_H__
#define __PARAMETERS_H__

#define HASH_TABLE_BUCKETS_SIZE 400
// The symbolic constant below should be the name of a hash function declared in
// `hashtable/hash.h`
#define HASH_TABLE_HASH_FUNCTION fnv_1a_hash
// Number of bytes in a MB.
#define MB_IN_BYTES 1000000
// Memory limit in bytes. ~130MB
#define MEMORY_LIMIT_IN_BYTES (130 * MB_IN_BYTES)

#endif
