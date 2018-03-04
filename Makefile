SRC = tpsd.c
CC = gcc
CFLAGS = -std=gnu99
LDFLAGS = -ludev

TARGET = tpsd

all: daemon

nodaemon: $(SRC)
	$(CC) -Wall $(CFLAGS) -o tpsd $(SRC) $(LDFLAGS)

daemon: $(SRC)
	$(CC) -Wall $(CFLAGS) -o tpsd $(SRC) $(LDFLAGS) -DDAEMONIZE

clean:
	rm -f $(TARGET)

install:
	cp $(TARGET) /usr/bin

