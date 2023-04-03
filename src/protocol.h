#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stddef.h>    // for size_t
#include <sys/epoll.h> // for struct epoll_event

#include "epoll.h"        // for struct EventData
#include "worker_state.h" // for struct WorkerArgs

#define CLIENT_READ_ERROR -1001
#define CLIENT_READ_CLOSED 0
#define CLIENT_READ_SUCCESS 1001
#define CLIENT_READ_INCOMPLETE 1002

#define CLIENT_WRITE_ERROR -2001
#define CLIENT_WRITE_SUCCESS 2001
#define CLIENT_WRITE_INCOMPLETE 2002

// Adds the given client event back to the epoll interest list. Returns 0 if
// successful, -1 otherwise.
int epoll_mod_client(int epoll_fd, struct epoll_event *event,
                     uint32_t event_flag);

// Writes the given buffer with the given size into the client socket's file
// descriptor, assuming that the amount of bytes in the value pointed at by
// `total_bytes_written` were already sent. If the whole buffer is correctly
// sent then CLIENT_WRITE_SUCCESS is returned and the value pointed at by
// `total_bytes_written` is updated to reflect this. If it's not possible to
// write the whole buffer, the value pointed at by `total_bytes_written` is
// updated to the new amount written and CLIENT_WRITE_INCOMPLETE is
// returned. If an error happens then CLIENT_WRITE_ERROR is returned.
int write_buffer(int fd, char *buffer, size_t buffer_length,
                 size_t *total_bytes_written);

// Reads from the given file descriptor into the given buffer up to the given
// size, keeping track of the total bytes read in total_bytes_read. Returns
// CLIENT_READ_ERROR if an error happens, CLIENT_READ_CLOSED if the client
// closes the connection, CLIENT_READ_INCOMPLETE if the file descriptor is not
// yet ready to finish reading, or CLIENT_READ_SUCCESS if the read was
// successfully finished.
int read_buffer(int fd, char *buffer, size_t buffer_size,
                size_t *total_bytes_read);

// Handles the STATS command and mutates the EventData instance accordingly.
void handle_stats(struct EventData *event_data, struct WorkerArgs *args);

// Handles the DEL command and mutates the EventData instance accordingly.
// WARNING: does not free the `key` pointer.
void handle_del(struct EventData *event_data, struct WorkerArgs *args,
                struct BoundedData *key);

// Handles the GET command and mutates the EventData instance accordingly.
// WARNING: does not free the `key` pointer.
void handle_get(struct EventData *event_data, struct WorkerArgs *args,
                struct BoundedData *key);

// Handles the TAKE command and mutates the EventData instance accordingly.
// WARNING: does not free the `key` pointer.
void handle_take(struct EventData *event_data, struct WorkerArgs *args,
                 struct BoundedData *key);

// Handles the PUT command and mutates the EventData instance accordingly.
// WARNING: the key and value pointer are owned by the hash table after the
// operation.
void handle_put(struct EventData *event_data, struct WorkerArgs *args,
                struct BoundedData *key, struct BoundedData *value);

#endif