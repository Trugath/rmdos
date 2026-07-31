OUTPUT_FORMAT(elf32-i386)
ENTRY(reset_vector)

/*
 * U18 as F000-segment offsets 0x8000–0xFFFF (linear 0xF8000–0xFFFFF).
 * IVT stores these 16-bit offsets with segment F000.
 */

SECTIONS
{
  . = 0x8000;

  .text : {
    *(.text)
    *(.data)
    *(.rodata)
  }

  ASSERT(. <= 0xE05B, "BIOS .text overflowed into POST entry")

  . = 0xE05B;
  .post : { *(.text.post) }

  . = 0xE842;
  .f1 : { *(.text.f1) }

  . = 0xEA82;
  .cad : { *(.text.cad) }

  . = 0xF065;
  .int10entry : { *(.text.int10entry) }

  . = 0xFA6E;
  .font : { *(.rodata.font) }

  ASSERT(. <= 0xFFF0, "BIOS font overflowed into reset vector")

  . = 0xFFF0;
  .reset : { *(.text.reset) }

  . = 0x10000;

  /DISCARD/ : {
    *(.bss*)
    *(COMMON)
    *(.comment*)
    *(.eh_frame*)
    *(.note*)
  }
}
