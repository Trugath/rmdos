.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 10h AH=08 read char/attr after AH=09 write */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0x0003
    int 0x10

    mov ah, 0x02
    xor bh, bh
    mov dx, 0x050A
    int 0x10

    mov ah, 0x09
    mov al, 'Q'
    mov bh, 0
    mov bl, 0x1E
    mov cx, 1
    int 0x10

    mov ah, 0x08
    xor bh, bh
    int 0x10
    cmp al, 'Q'
    jne .fail
    cmp ah, 0x1E
    jne .fail

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail:
    push cs
    pop ds
    mov si, offset msg
    call fail_and_halt

name:
    .asciz "bt_readchar"
msg:
    .asciz "bt_readchar:ch"

.include "firmware/bios/tests/boot/common.inc"
