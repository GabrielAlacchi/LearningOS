all: os.iso

kernel: kernel/kernel
	@echo "\nKernel build complete...\n"

bootloader/bootstrap.o:
	make -C bootloader

kernel/kernel: bootloader/bootstrap.o
	make -C kernel

iso/boot/grub:
	mkdir -p iso/boot/grub

os.iso: kernel/kernel bootloader/bootstrap.o iso/boot/grub bootloader/grub.cfg
	cp kernel/kernel iso/boot
	cp bootloader/grub.cfg iso/boot/grub
	grub-mkrescue -o os.iso iso/

clean:
	make -C kernel clean
	make -C bootloader clean
	rm os.iso

run: os.iso
	qemu-system-x86_64 -cdrom os.iso -serial stdio -m 1024M

debug:
	qemu-system-x86_64 -cdrom os.iso -serial stdio -m 1024M -s -S

test: FORCE
	make -C test

FORCE:
