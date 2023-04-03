memcached: $(wildcard *.c) $(wildcard *.h)
	gcc -O2 -pedantic -pthread -Wall -Werror -o memcached main.c worker_state.c worker_thread.c binary_type.c protocol.c text_protocol.c binary_protocol.c epoll.c sockets.c utils.c bounded_data.c hashtable.c

clean:
	rm -f *.o memcached
