#include <errno.h>     // for errno
#include <stdio.h>     // for perror
#include <sys/types.h> // for ssize_t
#include <unistd.h>    // for write

#include "protocol.h"

// Returns a string representation of the Binary Type.
char *binary_type_str(enum BinaryType binary_type) {
  switch (binary_type) {
  case BT_PUT:
    return "PUT";
  case BT_DEL:
    return "DEL";
  case BT_GET:
    return "GET";
  case BT_TAKE:
    return "TAKE";
  case BT_STATS:
    return "STATS";
  case BT_OK:
    return "OK";
  case BT_EINVAL:
    return "EINVAL";
  case BT_ENOTFOUND:
    return "ENOTFOUND";
  case BT_EBINARY:
    return "EBINARY";
  case BT_EBIG:
    return "EBIG";
  case BT_EUNK:
    return "EUNK";
  default:
    return "UNKNOWN_BINARY_TYPE";
  }
}

#define CLIENT_WRITE_ERROR -1
#define CLIENT_WRITE_SUCCESS 1
#define CLIENT_WRITE_INCOMPLETE 2

// Writes the given buffer with the given size into the client socket's file
// descriptor, assuming that the amount of bytes in the value pointed at by
// `total_bytes_written` were already sent. If the whole buffer is correctly
// sent then CLIENT_WRITE_SUCCESS is returned and the value pointed at by
// `total_bytes_written` is updated to reflect this. If it's not possible to
// write the whole buffer, the value pointed at by `total_bytes_written` is
// updated to the new amount written and CLIENT_WRITE_INCOMPLETE is returned. If
// an error happens then CLIENT_WRITE_ERROR is returned.
int write_buffer(int fd, char *buffer, size_t buffer_length,
                 size_t *total_bytes_written) {
  while (*total_bytes_written < buffer_length) {
    char *remaining_buffer = buffer + *total_bytes_written;
    size_t remaining_bytes = buffer_length - *total_bytes_written;
    ssize_t nwritten = write(fd, remaining_buffer, remaining_bytes);
    if (nwritten == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Client file descriptor is not ready to receive data, we should
        // continue later.
        return CLIENT_WRITE_INCOMPLETE;
      }

      // Another error happened.
      perror("write_buffer write");
      return CLIENT_WRITE_ERROR;
    }

    // TODO: remove this log after we test the resume behavior.
    if (nwritten != remaining_bytes) {
      printf("UNABLE TO WRITE ALL THE CONTENT, WE HAVE TO CONTINUE TRYING\n");
    }

    *total_bytes_written += nwritten;
  }

  return CLIENT_WRITE_SUCCESS;
}