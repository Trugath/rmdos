.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 16h AH=00 blocking read after AH=05 stuff */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov cx, 0x1E42              /* scan 1Eh, 'B' */
    mov ah, 0x05
    int 0x16
    jc .fail_stuff

    mov ah, 0x00
    int 0x16
    cmp ax, 0x1E42
    jne .fail_read

    mov ah, 0x01
    int 0x16
    jnz .fail_empty

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_stuff:
    push cs
    pop ds
    mov si, offset msg_stuff
    call fail_and_halt
.fail_read:
    push cs
    pop ds
    mov si, offset msg_read
    call fail_and_halt
.fail_empty:
    push cs
    pop ds
    mov si, offset msg_empty
    call fail_and_halt

name:
    .asciz "bt_kbd_read"
msg_stuff:
    .asciz "bt_kbd_read:stuff"
msg_read:
    .asciz "bt_kbd_read:read"
msg_empty:
    .asciz "bt_kbd_read:empty"

.include "firmware/bios/tests/boot/common.inc"
