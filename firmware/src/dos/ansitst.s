.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * ANSITST.COM — ANSI.SYS smoke: clear + yellow "ANSI OK" via AH=40.
 */

_start:
    mov ah, 0x40
    mov bx, 1                    /* stdout */
    mov cx, msg_end - msg
    lea dx, [msg]
    int 0x21
    mov ax, 0x4C00
    int 0x21

msg:
    .byte 0x1B
    .ascii "[2J"
    .byte 0x1B
    .ascii "[1;33m"
    .ascii "ANSI OK"
    .byte 0x1B
    .ascii "[0m"
    .ascii "\r\n"
msg_end:
