all: bootstrap.o

ASM = nasm
ASFLAGS = -f elf64

ASM_SRC = $(shell find . -type f -name "*.asm")

bootstrap.o: ${ASM_SRC}
	${ASM} bootstrap.asm ${ASFLAGS} -o $@

clean:
	rm *.o
