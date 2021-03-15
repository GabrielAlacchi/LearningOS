ENTRY(_start)

SECTIONS
{
  KERNEL_VMA = 0xFFFFFFFF80000000;

  . = 16M;
  __KERNEL_PHYSICAL_START = .;

  .multiboot : {
    ../bootloader/bootstrap.o (.multiboot)
  }

  .boottext : {
    ../bootloader/bootstrap.o (.text)
  }
  .bootrodata : {
    ../bootloader/bootstrap.o (.rodata)
  }
  .bootdata : {
    ../bootloader/bootstrap.o (.data)
  }
  .bootbss : {
    ../bootloader/bootstrap.o (pages.bss)
  }

  . = ALIGN(0x1000);
  _boot_end = .;

  . += KERNEL_VMA;
  __KERNEL_VIRTUAL_START = .;
  .text : AT(_boot_end)
  {
    *(.multiboot)
    *(.text)
  }

  . = ALIGN(0x1000);

  .eh_frame : AT ( (LOADADDR (.text) + SIZEOF (.text) + 0xFFF) & 0xFFFFFFFFFFFFF000 ) {
    *(.eh_frame*)
  }

  . = ALIGN(0x1000);

  .rodata : AT ( (LOADADDR (.eh_frame) + SIZEOF (.eh_frame) + 0xFFF) & 0xFFFFFFFFFFFFF000 )
  {
    *(.rodata)
  }

  __KERNEL_RODATA_START = (LOADADDR (.rodata) + 0xFFF) & 0xFFFFFFFFFFFFF000;
  __KERNEL_RODATA_END = (LOADADDR (.rodata) + SIZEOF (.rodata) + 0xFFF) & 0xFFFFFFFFFFFFF000;

  . = ALIGN(0x1000);

  .data : AT ( (LOADADDR (.rodata) + SIZEOF (.rodata) + 0xFFF) & 0xFFFFFFFFFFFFF000 )
  {
    *(.data)
  }

  . = ALIGN(0x1000);

  .bss : AT ( (LOADADDR (.data) + SIZEOF (.data) + 0xFFF) & 0xFFFFFFFFFFFFF000 )
  {
    *(COMMON)
    *(.bss)
  }

  __KERNEL_VIRTUAL_END = .;

  __KERNEL_PHYSICAL_END = (LOADADDR (.bss) + SIZEOF (.bss) + 0xFFF) & 0xFFFFFFFFFFFFF000;
}
