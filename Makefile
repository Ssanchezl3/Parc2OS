# Makefile — Smart Backup Kernel-Space Utility
#
# Targets:
#   make           → compila el binario ./backup
#   make clean     → elimina objetos y binario
#   make test      → copia rápida de /etc/hostname
#   make bench     → benchmark completo de los 3 métodos

CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -O2 -std=c11
LDFLAGS = -lrt
TARGET  = backup

SRCS = backup.c lib_io.c syscall_io.c syscall_buffered.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean test bench

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c lib_io.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(TARGET)
	./$(TARGET) /etc/hostname /tmp/hostname_bk.txt
	diff /etc/hostname /tmp/hostname_bk.txt && echo "[OK] Archivos identicos"
	@rm -f /tmp/hostname_bk.txt

bench: $(TARGET)
	./$(TARGET) --benchmark

clean:
	rm -f $(OBJS) $(TARGET) /tmp/bk_src_* /tmp/bk_dst_* /tmp/hostname_bk.txt
