CC=gcc

# gcc -O2 -pthread -Wall -Werror -o memcached main.c dispatcher.c worker.c epoll.c sockets.c utils.c bounded_data.c bounded_data_hashtable.c bucket.c hashtable.c command.c binary.c text.c

memcached: $(wildcard *.c) $(wildcard *.h)
	gcc -O2 -pthread -Wall -Werror -o memcached main.c worker_state.c worker_thread.c protocol.c text_protocol.c epoll.c sockets.c utils.c bounded_data.c bounded_data_hashtable.c bucket.c hashtable.c

# TODO: figure out the dependencies later... for now the rule above should work...

# memcached: main.o dispatcher.o worker.o
# 	$(CC) -o memcached main.o dispatcher.o worker.o -lpthread

# main.o: main.c dispatcher.o worker.o
# 	$(CC) -c -o main.o main.c

# dispatcher.o: dispatcher.c dispatcher.h worker.o epoll.o sockets.o
# 	$(CC) -c -o dispatcher.o dispatcher.c

# worker.o: worker.c worker.h epoll.o
# 	$(cc) -c -o worker.o worker.c

# sockets.o: sockets.c sockets.h
# 	$(CC) -c -o sockets.o sockets.c

# epoll.o: epoll.c epoll.h
# 	$(CC) -c -o epoll.o epoll.c

clean:
	rm -f *.o memcached
