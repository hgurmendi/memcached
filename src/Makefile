CC=gcc

memcached: main.o
	$(CC) -o memcached main.o

main.o: main.c
	$(CC) -c -o main.o main.c

clean:
	rm *.o memcached
