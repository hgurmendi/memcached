#ifndef __WRAPPED_FREE_H__
#define __WRAPPED_FREE_H__

#include <stdint.h>

extern uint64_t wrapped_free_mem;

void wrapped_free(void *ptr, uint64_t size);

#endif