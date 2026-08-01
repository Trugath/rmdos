.code16
.intel_syntax noprefix

/*
 * Pinned absolute entry points inside U18 (F000 segment offsets).
 * Bodies live in the main .text sections — these are tiny trampolines/tables.
 */

.section .text.post, "ax"
.global post_entry
post_entry:
    jmp post_main

/* IBM INT 19h entry */
.section .text.int19entry, "ax"
.global int19_entry
int19_entry:
    jmp int19_handler

/* IBM baud rate divisor table at E729; INT 14h trampoline follows at E739. */
.section .text.baud, "ax"
.global uart_divisors
uart_divisors:
    .word 1047                   /* 110 */
    .word 768                    /* 150 */
    .word 384                    /* 300 */
    .word 192                    /* 600 */
    .word 96                     /* 1200 */
    .word 48                     /* 2400 */
    .word 24                     /* 4800 */
    .word 12                     /* 9600 */

.section .text.int14entry, "ax"
.global int14_entry
int14_entry:
    jmp int14_handler

.section .text.f1, "ax"
.global f1_entry
f1_entry:
    /* k8086 auto-F1 keys off CGA "RESUME = ", not CS:IP E842. */
    jmp f1_wait

/* IBM INT 16h entry */
.section .text.int16entry, "ax"
.global int16_entry
int16_entry:
    jmp int16_handler

.section .text.cad, "ax"
.global cad_entry
cad_entry:
    /* IBM-compatible far jump; k8086 noticeWarmBoot keys off EA82 → E05B. */
    .byte 0xEA                  /* JMP FAR */
    .word 0xE05B                /* POST entry */
    .word 0xF000

/* IBM INT 13h floppy entry */
.section .text.int13entry, "ax"
.global int13_entry
int13_entry:
    jmp int13_handler

/* IBM INT 17h entry */
.section .text.int17entry, "ax"
.global int17_entry
int17_entry:
    jmp int17_handler

.section .text.int10entry, "ax"
.global int10_entry
int10_entry:
    /* IBM XT video entry F000:F065 — used by CheckIt and similar. */
    jmp int10_handler

/* IBM INT 12h / 11h / 15h */
.section .text.int12entry, "ax"
.global int12_entry
int12_entry:
    jmp int12_handler

.section .text.int11entry, "ax"
.global int11_entry
int11_entry:
    jmp int11_handler

.section .text.int15entry, "ax"
.global int15_entry
int15_entry:
    jmp int15_handler

.section .text.int1aentry, "ax"
.global int1a_entry
int1a_entry:
    jmp int1a_handler

.section .text.isr08entry, "ax"
.global isr08_entry
isr08_entry:
    jmp isr_08

.section .text.int5entry, "ax"
.global int5_entry
int5_entry:
    jmp int5_handler

.section .text.reset, "ax"
.global reset_vector
reset_vector:
    .byte 0xEA                  /* JMP FAR */
    .word 0xE05B                /* offset POST */
    .word 0xF000                /* segment */
    /* FFF5–FFFC: release date MM/DD/YY (8 chars); FFFD NUL; FFFE type; FFFF checksum */
    .ascii "08/01/26"
    .byte 0
    .byte 0xFE
    .byte 0
