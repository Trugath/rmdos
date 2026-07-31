.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 11h equipment + INT 12h memory size */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    int 0x11
    test ax, ax
    jz .fail_equip

    int 0x12
    cmp ax, 64
    jb .fail_mem

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_equip:
    push cs
    pop ds
    mov si, offset msg_equip
    call fail_and_halt

.fail_mem:
    push cs
    pop ds
    mov si, offset msg_mem
    call fail_and_halt

name:
    .asciz "bt_equip"
msg_equip:
    .asciz "bt_equip:INT11"
msg_mem:
    .asciz "bt_equip:INT12"

.include "firmware/bios/tests/boot/common.inc"
