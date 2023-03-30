#ifndef __TEXT_PROTOCOL_H__
#define __TEXT_PROTOCOL_H__

#include <sys/epoll.h>

#include "worker_state.h"

// Maximum request size for the text protocol.
#define MAX_TEXT_REQUEST_SIZE 2048

// Buffer size for the text protocol.
#define TEXT_REQUEST_BUFFER_SIZE (MAX_TEXT_REQUEST_SIZE + 2)

void handle_text_client_request(struct WorkerArgs *args,
                                struct epoll_event *event);

#endif