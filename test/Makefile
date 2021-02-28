all: run_all

CC = gcc
INC = -I../kernel/include -I../kernel/vendor -I./include
LIB = -lcmocka
ASM = nasm

KCCFLAGS = -nostdinc -nostdlib -ffreestanding -g -ggdb -Wall -MMD -mno-red-zone -DTESTSUITE
KSRC = $(shell find ../kernel -type f -name "*.c")
SRC = $(shell find ./src -type f -name "*.c")

KASM_SRC = $(shell find ../kernel -type f -name "*.asm")
KASFLAGS =  -f elf64

KOBJ = ${addprefix _build/kernel/, ${KASM_SRC:../kernel/%.asm=%.o}} ${addprefix _build/kernel/, ${KSRC:../kernel/%.c=%.o}}
OBJ = ${addprefix _build/suite/, ${SRC:./%.c=%.o}}
CCFLAGS = -g -Wall -ggdb

TEST_SUITE_SRCS = $(shell find ./suites -type f -name "*.c")
TEST_SUITE_BINS = ${TEST_SUITE_SRCS:.c=.test}


.PRECIOUS: ${OBJ} ${KOBJ}
.PHONY: run_tests

all_tests: ${TEST_SUITE_BINS}

run_all: all_tests run_tests
	for suite in ${TEST_SUITE_BINS}; do \
		$$suite; \
	done \

%.test: %.c ${OBJ} ${KOBJ}
	${CC} -o $@ ${CCFLAGS} ${INC} $^ -Tmetadefs.ld ${LIB}

_build/kernel/%.o: ../kernel/%.c
	mkdir -p ${dir $@}
	${CC} -c ${KCCFLAGS} ${INC} -o $@ $^

_build/kernel/%.o: ../kernel/%.asm
	mkdir -p ${dir $@}
	${ASM} $< ${KASFLAGS} -o $@

_build/suite/%.o: %.c
	mkdir -p ${dir $@}
	${CC} -c -DTESTSUITE ${CCFLAGS} ${INC} -o $@ $^

clean:
	rm -rf _build; \
	find -name *.test -delete;
