OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

/*
 * rmDesk small-model MZ:
 *   code MEMORY:  .text at VMA 0 (CS)
 *   data MEMORY:  .data + hole + .bss at VMA 0 (DS/SS)
 *   const MEMORY: .const at VMA 0 (blit_seg); LMA after para-aligned .data
 *
 * File image: code | data | pad | const.  BSS is NOLOAD after the const hole
 * so DS:bss does not overlap the loaded const bytes (same phys as blit_seg:0).
 */
MEMORY
{
  code (rx)  : ORIGIN = 0, LENGTH = 64K
  data (rw)  : ORIGIN = 0, LENGTH = 64K
  const (r)  : ORIGIN = 0, LENGTH = 64K
}

SECTIONS
{
  .text : {
    *(.text*)
  } > code AT > code

  .data : AT (ALIGN(LOADADDR(.text) + SIZEOF(.text), 16)) {
    *(.rodata*)
    *(.data*)
  } > data

  .const : AT (ALIGN(LOADADDR(.data) + SIZEOF(.data), 16)) {
    *(.const*)
  } > const

  .bss (NOLOAD) : {
    /*
     * After .data, LC is SIZEOF(.data). Skip the file bytes that follow
     * in the image (para pad + .const) so BSS begins past the loaded image.
     * Advance = (LMA_const - LMA_data) - SIZEOF(.data) + SIZEOF(.const).
     */
    . = . + (LOADADDR(.const) - LOADADDR(.data) - SIZEOF(.data)) + SIZEOF(.const);
    . = ALIGN(2);
    __bss_start = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(2);
    __bss_end = .;
  } > data

  desk_code_bytes = SIZEOF(.text);
  desk_data_bytes = SIZEOF(.data);
  desk_const_bytes = SIZEOF(.const);
  /* Actual BSS only (excludes DS hole that mirrors file pad + const). */
  desk_bss_bytes = __bss_end - __bss_start;
  desk_bss_end = __bss_end;
  /* Paragraphs from CS base to CONST segment base (runtime: CS + this). */
  desk_const_paras = (LOADADDR(.const) - LOADADDR(.text)) / 16;

  /DISCARD/ : {
    *(.comment*)
    *(.eh_frame*)
    *(.note*)
  }
}
