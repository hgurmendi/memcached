#ifndef __TEXT_PROTOCOL_H__
#define __TEXT_PROTOCOL_H__

#include <sys/epoll.h>

#include "worker_state.h"

void handle_text_client_request(struct WorkerArgs *args,
                                struct epoll_event *event);

#endif