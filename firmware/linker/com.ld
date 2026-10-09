OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

/* VMA 0x100 (COM), LMA 0 so objcopy -O binary is a clean .COM image. */
SECTIONS
{
  . = 0x0100;

  .text : AT(0)
  {
    /* Keep COM/EXE entry even when --gc-sections is used. */
    KEEP(*(.text._start))
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
