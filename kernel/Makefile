# Set the C compiler and debugger
CC = gcc
GDB = gdb

LD = ld
ASM = nasm

INC = -Iinclude/ -Ivendor/
SRC = $(shell find . -type f -name "*.c")
ASM_SRC = $(shell find . -type f -name "*.asm")

# CRITICAL: assembly must be linked first
OBJ = ${ASM_SRC:%.asm=_build/%.o} ${SRC:%.c=_build/%.o} ../bootloader/bootstrap.o

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

_build/%.o: %.c
	mkdir -p ${dir $@}
	${CC} -c ${CCFLAGS} ${INC} -o $@ $^

_build/%.o: %.asm
	mkdir -p ${dir $@}
	${ASM} $< ${ASFLAGS} -o $@

clean:
	rm kernel; rm -rf _build
