.code16
.intel_syntax noprefix
.section .text
.global _start

/* Mode 4: AH=09/0A/08 graphics-plane character I/O. */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0x0004
    int 0x10
    mov ah, 0x02
    xor bh, bh
    xor dx, dx
    int 0x10

    mov ah, 0x09
    mov al, 'A'
    mov bh, 0
    mov bl, 3
    mov cx, 1
    int 0x10

    mov ah, 0x0D
    xor bh, bh
    mov cx, 3
    mov dx, 1
    int 0x10
    test al, al
    jz .fail_pix

    mov ah, 0x08
    xor bh, bh
    int 0x10
    cmp al, 'A'
    jne .fail_rd

    mov ah, 0x0A
    mov al, 'B'
    mov bh, 0
    mov bl, 2
    mov cx, 1
    int 0x10
    mov ah, 0x08
    int 0x10
    cmp al, 'B'
    jne .fail_wco

    mov ax, 0x1301
    mov bx, 0x0003
    mov cx, 2
    mov dx, 0x0100
    push cs
    pop es
    mov bp, offset msg_hi
    int 0x10
    mov ah, 0x03
    xor bh, bh
    int 0x10
    cmp dx, 0x0102
    jne .fail_str

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_pix:
    push cs
    pop ds
    mov si, offset msg_pix
    call fail_and_halt
.fail_rd:
    push cs
    pop ds
    mov si, offset msg_rd
    call fail_and_halt
.fail_wco:
    push cs
    pop ds
    mov si, offset msg_wco
    call fail_and_halt
.fail_str:
    push cs
    pop ds
    mov si, offset msg_str
    call fail_and_halt

name:    .asciz "bt_gfx_char"
msg_hi:  .ascii "Hi"
msg_pix: .asciz "bt_gfx_char:pix"
msg_rd:  .asciz "bt_gfx_char:rd"
msg_wco: .asciz "bt_gfx_char:wco"
msg_str: .asciz "bt_gfx_char:str"

.include "firmware/bios/tests/boot/common.inc"
