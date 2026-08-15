.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * BIGEXE payload — printed after MZ pack+pad to ~75 KiB.
 * Linked as a COM body; pack_mz.py wraps and pads for streaming EXEC e2e.
 */

_start:
    mov ah, 0x09
    lea dx, [msg]
    int 0x21
    mov ax, 0x4C00
    int 0x21

msg:
    .ascii "BIGEXE OK\r\n$"
