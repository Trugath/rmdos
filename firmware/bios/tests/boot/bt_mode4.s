.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * Mode 4 (320x200) smoke: set mode, confirm graphics bit, write a nonzero
 * pattern into both CGA banks.
 */

.equ BDA_SEG, 0x0040
.equ BDA_CRT_MODE, 0x49
.equ BDA_CRT_MODE_REG, 0x65

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0x0004
    int 0x10

    mov ax, BDA_SEG
    mov es, ax
    cmp byte ptr es:[BDA_CRT_MODE], 4
    jne .fail_mode
    test byte ptr es:[BDA_CRT_MODE_REG], 0x02
    jz .fail_gfx

    mov ax, 0xB800
    mov es, ax
    mov byte ptr es:[0], 0xAA
    mov ax, 0xBA00
    mov es, ax
    mov byte ptr es:[0], 0x55

    mov ax, 0xB800
    mov es, ax
    cmp byte ptr es:[0], 0xAA
    jne .fail_vram
    mov ax, 0xBA00
    mov es, ax
    cmp byte ptr es:[0], 0x55
    jne .fail_vram

    /* return to text mode 3 */
    mov ax, 0x0003
    int 0x10
    mov ax, BDA_SEG
    mov es, ax
    test byte ptr es:[BDA_CRT_MODE_REG], 0x02
    jnz .fail_restore

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_mode:
    push cs
    pop ds
    mov si, offset msg_mode
    call fail_and_halt
.fail_gfx:
    push cs
    pop ds
    mov si, offset msg_gfx
    call fail_and_halt
.fail_vram:
    push cs
    pop ds
    mov si, offset msg_vram
    call fail_and_halt
.fail_restore:
    push cs
    pop ds
    mov si, offset msg_rest
    call fail_and_halt

name:
    .asciz "bt_mode4"
msg_mode:
    .asciz "bt_mode4:mode"
msg_gfx:
    .asciz "bt_mode4:gfx"
msg_vram:
    .asciz "bt_mode4:vram"
msg_rest:
    .asciz "bt_mode4:restore"

.include "firmware/bios/tests/boot/common.inc"
