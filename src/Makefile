CC=gcc

memcached: main.o
	$(CC) -o memcached main.o

main.o:
	$(CC) -c -o main.o main.c

clean:
	rm *.o memcached
