OUTPUT_FORMAT(elf32-i386)
ENTRY(reset_vector)

/*
 * U18 as F000-segment offsets 0x8000–0xFFFF (linear 0xF8000–0xFFFFF).
 * IVT stores these 16-bit offsets with segment F000.
 *
 * IBM absolute pins (trampolines / tables) sit between free gaps.
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

  . = 0xE6F2;
  .int19entry : { *(.text.int19entry) }

  . = 0xE729;
  .baud : { *(.text.baud) }

  . = 0xE739;
  .int14entry : { *(.text.int14entry) }

  . = 0xE82E;
  .int16entry : { *(.text.int16entry) }

  . = 0xE842;
  .f1 : { *(.text.f1) }

  . = 0xEA82;
  .cad : { *(.text.cad) }

  . = 0xEC59;
  .int13entry : { *(.text.int13entry) }

  . = 0xEFD2;
  .int17entry : { *(.text.int17entry) }

  . = 0xF065;
  .int10entry : { *(.text.int10entry) }

  . = 0xF841;
  .int12entry : { *(.text.int12entry) }

  . = 0xF84D;
  .int11entry : { *(.text.int11entry) }

  . = 0xF859;
  .int15entry : { *(.text.int15entry) }

  . = 0xFA6E;
  .font : { *(.rodata.font) }

  ASSERT(. <= 0xFE6E, "BIOS font overflowed into INT 1Ah entry")

  . = 0xFE6E;
  .int1aentry : { *(.text.int1aentry) }

  . = 0xFEA5;
  .isr08entry : { *(.text.isr08entry) }

  . = 0xFF54;
  .int5entry : { *(.text.int5entry) }

  ASSERT(. <= 0xFFF0, "INT 5 trampoline overflowed into reset vector")

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
