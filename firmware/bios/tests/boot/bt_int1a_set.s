.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 1Ah AH=01 set ticks; AH=00 reads back matching CX:DX */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ah, 0x01
    mov cx, 0x0012
    mov dx, 0x3456
    int 0x1A

    xor ah, ah
    int 0x1A
    /* Allow one IRQ0 tick of skew on DX */
    cmp cx, 0x0012
    jne .fail_hi
    mov ax, dx
    sub ax, 0x3456
    cmp ax, 2
    ja .fail_lo

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_hi:
    push cs
    pop ds
    mov si, offset msg_hi
    call fail_and_halt
.fail_lo:
    push cs
    pop ds
    mov si, offset msg_lo
    call fail_and_halt

name:
    .asciz "bt_int1a_set"
msg_hi:
    .asciz "bt_int1a_set:hi"
msg_lo:
    .asciz "bt_int1a_set:lo"

.include "firmware/bios/tests/boot/common.inc"
