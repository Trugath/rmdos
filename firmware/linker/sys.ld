OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

/* SYS driver: VMA/LMA 0 so objcopy binary starts with the device header. */
SECTIONS
{
  . = 0x0000;

  .text : AT(0)
  {
    *(.text*)
    *(.rodata*)
    *(.data*)
  }

  /DISCARD/ : {
    *(.bss*)
    *(COMMON)
    *(.comment*)
    *(.eh_frame*)
    *(.note*)
  }
}
