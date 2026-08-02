.code16
.intel_syntax noprefix
.section .text
.global _start

/* C800: write cyl1 then AH=04 verify */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ah, 0x00
    mov dl, 0x80
    int 0x13
    jc .fail_reset

    mov di, 0x9000
    mov cx, 256
    mov ax, 0x5AA5
.fill:
    stosw
    loop .fill

    mov ax, 0x0301
    mov bx, 0x9000
    mov cx, 0x0101
    xor dx, dx
    mov dl, 0x80
    int 0x13
    jc .fail_write

    mov ax, 0x0401
    mov bx, 0x9000
    mov cx, 0x0101
    xor dx, dx
    mov dl, 0x80
    int 0x13
    jc .fail_verify
    test ah, ah
    jnz .fail_verify

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_reset:
    push cs
    pop ds
    mov si, offset msg_reset
    call fail_and_halt
.fail_write:
    push cs
    pop ds
    mov si, offset msg_write
    call fail_and_halt
.fail_verify:
    push cs
    pop ds
    mov si, offset msg_verify
    call fail_and_halt

name:
    .asciz "bt_hd_verify"
msg_reset:
    .asciz "bt_hd_verify:rst"
msg_write:
    .asciz "bt_hd_verify:wr"
msg_verify:
    .asciz "bt_hd_verify:vf"

.include "firmware/bios/tests/boot/common.inc"
