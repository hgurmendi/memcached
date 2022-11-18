#include "epoll.h"

struct ClientEpollEventData *get_event_data(struct epoll_event event) {
  return (struct ClientEpollEventData *)(event.data.ptr);
}

int is_epoll_error(struct epoll_event event) {
  return (event.events & EPOLLERR) || (event.events & EPOLLHUP) ||
         !(event.events & EPOLLIN);
}