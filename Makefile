SRC = tpsd.c
CC = gcc
CFLAGS = -std=gnu99
LDFLAGS = -ludev

TARGET = tpsd

all: $(TARGET)

tpsd: $(SRC)
	$(CC) -Wall $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)

install:
	cp $(TARGET) /usr/bin

