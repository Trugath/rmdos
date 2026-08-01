.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 10h AH=0E BEL (AL=07) returns; following teletype still works */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0x0003
    int 0x10

    mov ah, 0x0E
    mov al, 0x07
    mov bh, 0
    int 0x10

    mov ah, 0x0E
    mov al, 'B'
    mov bh, 0
    int 0x10

    mov ax, 0xB800
    mov es, ax
    cmp byte ptr es:[0], 'B'
    jne .fail_tty

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_tty:
    push cs
    pop ds
    mov si, offset msg_tty
    call fail_and_halt

name:
    .asciz "bt_bel"
msg_tty:
    .asciz "bt_bel:tty"

.include "firmware/bios/tests/boot/common.inc"
