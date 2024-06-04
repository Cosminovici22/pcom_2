CC := gcc
CFLAGS := -Wall -Wextra

.PHONY: build server subscriber clean

build: server subscriber

server: utils.o server.o queue.o
	$(CC) $(CFLAGS) $^ -o $@

subscriber: utils.o subscriber.o queue.o
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o server subscriber
