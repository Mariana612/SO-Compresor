CC      = gcc
CFLAGS  = -std=c11 -D_POSIX_C_SOURCE=200809L -O2 -Wall -Wextra -pedantic -I Code/Commons
LDLIBS  = -lcrypto

COMMONS = Code/Commons/Cli.c Code/Commons/Codec.c Code/Commons/HuffmanTree.c \
          Code/Commons/MD5Utils.c Code/Commons/FileList.c
HEADERS = $(wildcard Code/Commons/*.h)

.PHONY: all serial fork pthread clean

all: serial fork pthread

serial: huffman-serial
fork: huffman-fork
pthread: huffman-pthread

huffman-serial: $(wildcard Code/Serial/*.c Code/Serial/*.h) $(COMMONS) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ Code/Serial/*.c $(COMMONS) $(LDLIBS)

huffman-fork: $(wildcard Code/Fork/*.c Code/Fork/*.h) $(COMMONS) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ Code/Fork/*.c $(COMMONS) $(LDLIBS)

huffman-pthread: $(wildcard Code/Pthread/*.c Code/Pthread/*.h) $(COMMONS) $(HEADERS)
	$(CC) $(CFLAGS) -pthread -o $@ Code/Pthread/*.c $(COMMONS) $(LDLIBS)

clean:
	rm -f huffman-serial huffman-fork huffman-pthread
