all: binder memcached

memcached: $(wildcard *.c) $(wildcard *.h)
	gcc -O2 -pedantic -pthread -Wall -Werror -o memcached main.c worker_state.c worker_thread.c binary_type.c protocol.c text_protocol.c binary_protocol.c epoll.c sockets.c utils.c bounded_data.c hashtable.c

binder: binder.c sockets.c
	gcc -O2 -pedantic -Wall -Werror -o binder binder.c sockets.c

binder_test: binder_test.c
	gcc -O2 -pedantic -Wall -Werror -o binder_test binder_test.c

drop_privileges_test: binder binder_test

clean:
	rm -f memcached binder
