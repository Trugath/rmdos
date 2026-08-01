.code16
.intel_syntax noprefix
.section .text
.global _start

/* Mode 6 (640x200) INT 10h AH=0C/0D pixel round-trip */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0x0006
    int 0x10

    mov ah, 0x0C
    mov al, 0x01
    xor bh, bh
    mov cx, 100
    mov dx, 50
    int 0x10

    mov ah, 0x0D
    xor bh, bh
    mov cx, 100
    mov dx, 50
    int 0x10
    cmp al, 0x01
    jne .fail_on

    /* Clear pixel */
    mov ah, 0x0C
    xor al, al
    mov cx, 100
    mov dx, 50
    int 0x10
    mov ah, 0x0D
    mov cx, 100
    mov dx, 50
    int 0x10
    cmp al, 0
    jne .fail_off

    mov ax, 0x0003
    int 0x10

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_on:
    push cs
    pop ds
    mov si, offset msg_on
    call fail_and_halt
.fail_off:
    push cs
    pop ds
    mov si, offset msg_off
    call fail_and_halt

name:
    .asciz "bt_pixel6"
msg_on:
    .asciz "bt_pixel6:on"
msg_off:
    .asciz "bt_pixel6:off"

.include "firmware/bios/tests/boot/common.inc"
