#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stddef.h> // for size_t

#define CLIENT_WRITE_ERROR -1
#define CLIENT_WRITE_SUCCESS 1
#define CLIENT_WRITE_INCOMPLETE 2

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

// Returns a string representation of the Binary Type.
char *binary_type_str(enum BinaryType binary_type);

// Writes the given buffer with the given size into the client socket's file
// descriptor, assuming that the amount of bytes in the value pointed at by
// `total_bytes_written` were already sent. If the whole buffer is correctly
// sent then CLIENT_WRITE_SUCCESS is returned and the value pointed at by
// `total_bytes_written` is updated to reflect this. If it's not possible to
// write the whole buffer, the value pointed at by `total_bytes_written` is
// updated to the new amount written and CLIENT_WRITE_INCOMPLETE is returned. If
// an error happens then CLIENT_WRITE_ERROR is returned.
int write_buffer(int fd, char *buffer, size_t buffer_length,
                 size_t *total_bytes_written);

#endif