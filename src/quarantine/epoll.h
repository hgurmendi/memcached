#ifndef __EPOLL_H__
#define __EPOLL_H__

#include <netdb.h>
#include <sys/epoll.h>

#define MAX_EVENTS 64

// @TODO: Change this
// This is a struct whose memory should be requested by the dispatcher
// and it should be assigned to epoll_event.data.ptr (which is a pointer to
// void) and it allows us to store arbitrary data accessible when the event is
// triggered in an epoll wait
struct ClientEpollEventData {
  // File descriptor of the client.
  int fd;
  // Connection type of the client.
  enum ConnectionTypes connection_type;
  // IP of the client.
  char host[NI_MAXHOST];
  // Port of the client.
  char port[NI_MAXSERV];
};

/* Properly extracts a pointer to the ClientEpollEventData from the actual
 * `epoll_event` struct.
 */
struct ClientEpollEventData *get_event_data(struct epoll_event event);

/* Returns true is a "transient" epoll error happened on the event's file
 * descriptor.
 */
int is_epoll_error(struct epoll_event event);

#endif