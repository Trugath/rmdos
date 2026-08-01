.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 16h AH=02 shift flags; AH=01 empty buffer → ZF */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ah, 0x02
    int 0x16
    /* AL = shift flags; any value is fine if call returns */

    /* AH=02 reflects BDA FLAG0 (Num/Scroll/Ins/Caps bits). */
    mov ax, 0x0040
    mov es, ax
    mov byte ptr es:[0x17], 0x70     /* Caps|Num|Scroll */
    mov ah, 0x02
    int 0x16
    cmp al, 0x70
    jne .fail_flags
    mov byte ptr es:[0x17], 0

    mov ah, 0x01
    int 0x16
    jnz .fail_not_empty

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_not_empty:
    push cs
    pop ds
    mov si, offset msg_empty
    call fail_and_halt
.fail_flags:
    push cs
    pop ds
    mov si, offset msg_flags
    call fail_and_halt

name:
    .asciz "bt_kbd_flags"
msg_empty:
    .asciz "bt_kbd_flags:empty"
msg_flags:
    .asciz "bt_kbd_flags:flags"

.include "firmware/bios/tests/boot/common.inc"
