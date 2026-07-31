.code16
.intel_syntax noprefix

/*
 * Pinned absolute entry points inside U18 (F000 segment offsets).
 * Bodies live in bios_main.s — these are tiny trampolines.
 */

.section .text.post, "ax"
.global post_entry
post_entry:
    jmp post_main

.section .text.f1, "ax"
.global f1_entry
f1_entry:
    /* k8086 auto-F1 keys off CGA "RESUME = ", not CS:IP E842. */
    jmp f1_wait

.section .text.cad, "ax"
.global cad_entry
cad_entry:
    /* IBM-compatible far jump; k8086 noticeWarmBoot keys off EA82 → E05B. */
    .byte 0xEA                  /* JMP FAR */
    .word 0xE05B                /* POST entry */
    .word 0xF000

.section .text.int10entry, "ax"
.global int10_entry
int10_entry:
    /* IBM XT video entry F000:F065 — used by CheckIt and similar. */
    jmp int10_handler

.section .text.reset, "ax"
.global reset_vector
reset_vector:
    .byte 0xEA                  /* JMP FAR */
    .word 0xE05B                /* offset POST */
    .word 0xF000                /* segment */
    /* Pad to end of 16-byte reset window */
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0x00, 0x00
