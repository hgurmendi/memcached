#ifndef __BINARY_PROTOCOL_H__
#define __BINARY_PROTOCOL_H__

#include <sys/epoll.h> // for struct epoll_event

#include "worker_state.h" // for struct WorkerArgs

int handle_binary_client_response(struct WorkerArgs *args,
                                  struct epoll_event *event);

int handle_binary_client_request(struct WorkerArgs *args,
                                 struct epoll_event *event);

#endif