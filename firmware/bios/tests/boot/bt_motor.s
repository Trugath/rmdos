.code16
.intel_syntax noprefix
.section .text
.global _start

/* Floppy motor-on then IRQ0 timeout clears BDA motor bits */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    mov [0x7e00], dl
    sti

    /* Read boot sector → fdc_motor_on */
    mov ax, 0x0201
    mov bx, 0x9000
    mov cx, 0x0001
    mov dh, 0
    mov dl, [0x7e00]
    int 0x13
    jc .fail_read

    mov ax, 0x40
    mov ds, ax
    test byte ptr [0x3E], 0xF0
    jz .fail_on

    /* Wait > motor-off delay ticks (DPT +2 ≈ 0x25) */
    mov bx, [0x6C]
.wait:
    mov ax, [0x6C]
    sub ax, bx
    cmp ax, 0x30
    jb .wait
    test byte ptr [0x3E], 0xF0
    jnz .fail_off

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_read:
    push cs
    pop ds
    mov si, offset msg_read
    call fail_and_halt
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
    .asciz "bt_motor"
msg_read:
    .asciz "bt_motor:read"
msg_on:
    .asciz "bt_motor:on"
msg_off:
    .asciz "bt_motor:off"

.include "firmware/bios/tests/boot/common.inc"
