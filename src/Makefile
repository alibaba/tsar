CFLAGS = -g -O2 -Wall
CC = gcc
INCLUDE_DIR = ../include


%.o: %.c
	$(CC) -o $@ -c -I$(INCLUDE_DIR) $(CFLAGS) $<

all: tsar

tsar: config.o debug.o framework.o tsar.o output_file.o output_print.o output_db.o output_tcp.o output_nagios.o common.o
	$(CC) config.o debug.o framework.o tsar.o output_file.o output_print.o output_db.o output_tcp.o output_nagios.o common.o -o tsar -I$(INCLUDE_DIR) -g -Wall -ldl -rdynamic

clean:
	rm -rf *.o tsar
