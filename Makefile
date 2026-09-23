CC      = gcc
CFLAGS  = -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -pedantic -I src/common
LDLIBS  = -lcrypto
BIN     = bin

COMMON  = $(wildcard src/common/*.c)
HEADERS = $(wildcard src/common/*.h)

.PHONY: all serial fork pthread gui clean

all: serial fork pthread

serial: $(BIN)/huffman-serial
fork: $(BIN)/huffman-fork
pthread: $(BIN)/huffman-pthread
gui: $(BIN)/interfaz

$(BIN)/huffman-serial: $(wildcard src/serial/*.c src/serial/*.h) $(COMMON) $(HEADERS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ src/serial/*.c $(COMMON) $(LDLIBS)

$(BIN)/huffman-fork: $(wildcard src/fork/*.c src/fork/*.h) $(COMMON) $(HEADERS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ src/fork/*.c $(COMMON) $(LDLIBS)

$(BIN)/huffman-pthread: $(wildcard src/pthread/*.c src/pthread/*.h) $(COMMON) $(HEADERS) | $(BIN)
	$(CC) $(CFLAGS) -pthread -o $@ src/pthread/*.c $(COMMON) $(LDLIBS)

# Interfaz gráfica (GTK 4)
$(BIN)/interfaz: $(wildcard src/gui/*.c src/gui/*.h) src/common/Stats.c src/common/Stats.h | $(BIN)
	$(CC) $(CFLAGS) -o $@ src/gui/*.c src/common/Stats.c $$(pkg-config --cflags --libs gtk4) $(LDLIBS)

$(BIN):
	mkdir -p $@

clean:
	rm -f $(BIN)/huffman-serial $(BIN)/huffman-fork $(BIN)/huffman-pthread $(BIN)/interfaz
