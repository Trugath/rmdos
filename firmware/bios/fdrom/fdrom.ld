ENTRY(_start)
OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
SECTIONS
{
  . = 0;
  .text : {
    *(.text)
    *(.text.*)
    *(.data)
    *(.data.*)
    *(.rodata*)
    *(.bss)
    *(.bss.*)
  }
  /* Scratch for verify; host finds geo via magic in .text (see pack / Machine) */
  . = 0x500;
  .vbuf : { *(.vbuf) }
  . = 0x800;
}
