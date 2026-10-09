.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * HELLO.COM — tiny DOS program for rmDOS loader E2E.
 * Linked at offset 0; loaded to PSP:0100.
 */

_start:
    mov ah, 0x09
    lea dx, [msg]
    int 0x21
    mov ax, 0x4C00
    int 0x21

msg:
    .ascii "HELLO\r\n$"
