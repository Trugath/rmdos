OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS
{
  . = 0x7c00;

  .text :
  {
    *(.text*)
  }

  .rodata :
  {
    *(.rodata*)
  }

  .data :
  {
    *(.data*)
  }

  . = 0x7dfe;

  .sig :
  {
    SHORT(0xaa55)
  }

  /DISCARD/ :
  {
    *(.bss*)
    *(.comment*)
    *(.eh_frame*)
    *(.note*)
  }
}
