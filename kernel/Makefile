# Set the C compiler and debugger
CC = x86_64-elf-gcc
GDB = gdb

LD = x86_64-elf-ld
ASM = nasm

INC = -Iinclude/
SRC = $(shell find . -type f -name "*.c")
ASM_SRC = $(shell find . -type f -name "*.asm")

# CRITICAL: assembly must be linked first
OBJ = ${ASM_SRC:.asm=.o} ${SRC:.c=.o} 

CCFLAGS = -ggdb -c -ffreestanding -mno-red-zone -fno-pic
LDFLAGS = -Tkernel.ld
LDFLAGS_BIN = ${LDFLAGS} --oformat binary
ASFLAGS = -f elf64

all: kernel kernel.elf

kernel: ${OBJ}
	${LD} -o $@ ${LDFLAGS_BIN} $^

kernel.elf: ${OBJ}
	${LD} -o $@ ${LDFLAGS} $^

%.o: %.c
	${CC} ${CCFLAGS} ${INC} -o $@ $^

%.o: %.asm
	${ASM} $< ${ASFLAGS} -o $@

clean:
	rm -f kernel kernel.elf *.o **/*.o
