# Makefile for blackhole (adjust RAYLIB_DIR if necessary)
RAYLIB_DIR ?= raylib/src
CC = gcc
CFLAGS = -O2 -Wall -I$(RAYLIB_DIR)
LDFLAGS = -L$(RAYLIB_DIR) -l:libraylib.a -lm
TARGET = blackhole
SRC = blackhole.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) disk.frag lens.frag nebula.frag

