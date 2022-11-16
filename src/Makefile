CC=gcc

memcached: main.o
	$(CC) -o memcached main.o sockets.o

main.o: main.c sockets.o
	$(CC) -c -o main.o main.c

sockets.o: sockets.c sockets.h
	$(CC) -c -o sockets.o sockets.c

clean:
	rm *.o memcached
