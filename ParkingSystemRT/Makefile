# Makefile - compila controller, client e display
CC=gcc
CFLAGS=-Wall -Wextra -g -Isrc
LDFLAGS=-lrt -lncurses -pthread

SRC_DIR=src
BUILD_DIR=build

all: dirs controller client display

dirs:
	mkdir -p $(BUILD_DIR)

controller: $(SRC_DIR)/controller.c $(SRC_DIR)/common.h
	$(CC) $(CFLAGS) $(SRC_DIR)/controller.c -o $(BUILD_DIR)/controller $(LDFLAGS)

client: $(SRC_DIR)/client.c $(SRC_DIR)/common.h
	$(CC) $(CFLAGS) $(SRC_DIR)/client.c -o $(BUILD_DIR)/client $(LDFLAGS)

display: $(SRC_DIR)/display.c $(SRC_DIR)/common.h
	$(CC) $(CFLAGS) $(SRC_DIR)/display.c -o $(BUILD_DIR)/display $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) *.o
	rm -f /dev/shm/*park_shm* || true