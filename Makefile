CC=gcc
CFLAGS=-Wall

all: src/crc16.o src/notification.o
	$(CC) $(CFLAGS) -o notification $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o
	rm -f notification
