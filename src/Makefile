all: binder memcached

drop_privileges_test: binder binder_test

memcached: $(wildcard *.c) $(wildcard *.h)
	gcc -O2 -pedantic -pthread -Wall -Werror -o memcached main.c worker_state.c worker_thread.c binary_type.c protocol.c text_protocol.c binary_protocol.c epoll.c sockets.c utils.c bounded_data.c hashtable.c

binder: binder.c sockets.c
	gcc -O2 -pedantic -Wall -Werror binder.c sockets.c -o binder

binder_test: binder_test.c
	gcc -O2 -pedantic -Wall -Werror binder_test.c -o binder_test

clean:
	rm -f memcached binder
