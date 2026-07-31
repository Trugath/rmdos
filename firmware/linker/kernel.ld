OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS
{
  . = 0x0000;

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

  .bss :
  {
    *(.bss*)
    *(COMMON)
  }

  /DISCARD/ :
  {
    *(.comment*)
    *(.eh_frame*)
    *(.note*)
  }
}
