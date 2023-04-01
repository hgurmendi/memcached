#ifndef __TEXT_PROTOCOL_H__
#define __TEXT_PROTOCOL_H__

#include <sys/epoll.h>

#include "worker_state.h"

int handle_text_client_response(struct WorkerArgs *args,
                                struct epoll_event *event);

void handle_text_client_request(struct WorkerArgs *args,
                                struct epoll_event *event);

#endif