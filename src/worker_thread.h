#ifndef __WORKER_THREAD_H__
#define __WORKER_THREAD_H__

enum BinaryType {
  BT_PUT = 11,
  BT_DEL = 12,
  BT_GET = 13,
  BT_TAKE = 14,
  BT_STATS = 21,
  BT_OK = 101,
  BT_EINVAL = 111,
  BT_ENOTFOUND = 112,
  BT_EBINARY = 113,
  BT_EBIG = 114,
  BT_EUNK = 115,
};

// Worker thread function.
void *worker(void *_args);

// Returns a string representation of the Binary Type.
char *binary_type_str(enum BinaryType command);

#endif