# soi-memcached

A programming assignment for the Operating Systems I subject for the Computer Science degree in
Universidad Nacional de Rosario: a memcached implementation in C.

[Memcached](https://memcached.org/) is an in-memory key-value store for small chunks of arbitrary
data (strings, objects) from results of database calls, API calls, or page rendering.

# References

- Used the following tutorial for `epoll`: https://web.archive.org/web/20120504033548/https://banu.com/blog/2/how-to-use-epoll-a-complete-example-in-c/
- Used the following tutorial for POSIX threads: https://www.cs.cmu.edu/afs/cs/academic/class/15492-f07/www/pthreads.html

# Docker instructions

There is a `Dockerfile` for running the project inside a Docker container in case you're using
an operating system that doesn't support epoll, which is only available in Linux.

In order to run the Docker container you should run the following commands, assuming you have
Docker installed in your system:

```bash
$ docker build -t soi-memcached .
$ docker run -p 8000:8000 --rm -it --name memcached soi-memcached
```

Or you can run a script that does that for you:

```bash
$ ./run_in_docker.sh
```

Bear in mind that you'll have to stop the container, build and run it again after changes.
