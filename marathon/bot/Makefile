CC=gcc
CFLAGS=-I.
DEPS = util.h list.h pickle.h sock_client.h

%.o: %.c $(DEPS)
	$(CC) -std=c99 -D_POSIX_C_SOURCE=200112L -ggdb -c -o $@ $< $(CFLAGS)
main: main.o util.o pickle.o list.o sock_client.o
	$(CC) -o main main.o pickle.o list.o util.o sock_client.o -std=c99 -D_POSIX_C_SOURCE=200112L -ggdb -I.
