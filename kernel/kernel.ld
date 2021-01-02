ENTRY(_start)

SECTIONS
{
  . = 0x8200;

  /* 4K of code */
  .text : {
      *(.text)
      *(.text.*)
  }

  /* 4K of Read-only data */
  .rodata : ALIGN(0x1000)
  {
      *(.rodata*)
  }

  /* 4K of Read-write initialized data. */
  .data : ALIGN(0x1000)
  {
      *(.data*)
  }

  /* 4K of Read-write uninitialized data. */
  .bss : ALIGN(0x1000)
  {
      *(.bss*)
  }

  __KERNEL_END = .;
}
