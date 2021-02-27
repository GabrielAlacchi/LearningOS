# Set the C compiler and debugger
CC = gcc
GDB = gdb

LD = ld
ASM = nasm

INC = -Iinclude/ -Ivendor/
SRC = $(shell find . -type f -name "*.c")
ASM_SRC = $(shell find . -type f -name "*.asm")

# CRITICAL: assembly must be linked first
OBJ = ${ASM_SRC:.asm=.o} ${SRC:.c=.o} ../bootloader/bootstrap.o

CCFLAGS = -ggdb -nostdinc -nostdlib -ffreestanding -g -Wall -Wextra -I. -MMD -mno-red-zone -mcmodel=kernel -fno-pie
LDFLAGS = -Tkernel.ld -melf_x86_64
LDFLAGS_BIN = ${LDFLAGS}
ASFLAGS = -f elf64
DFILES = $(patsubst %.o,%.d,$(OBJ))

all: kernel

../bootloader/bootstrap.o:
	make -C ../bootloader

kernel: ${OBJ} kernel.ld Makefile
	${LD} -o $@ ${LDFLAGS} ${OBJ}

%.o: %.c
	${CC} -c ${CCFLAGS} ${INC} -o $@ $^

%.o: %.asm
	${ASM} $< ${ASFLAGS} -o $@

clean:
	find -name *.o -delete; find -name *.d -delete; rm kernel

