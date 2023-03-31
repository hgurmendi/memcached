#include "binary_protocol.h"
#include "epoll.h"

void handle_binary_client_request(struct WorkerArgs *args,
                                  struct epoll_event *event) {
  struct EventData *event_data = event->data.ptr;

  // TODO: IMPLEMENT handling of binary client request. Below is just for the
  // compiler to stop whining.
  event_data->fd = event_data->fd;
}