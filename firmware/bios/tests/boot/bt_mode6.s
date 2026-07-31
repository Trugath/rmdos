.code16
.intel_syntax noprefix
.section .text
.global _start

/* Mode 6 (640x200): graphics + hi-res bits, bank write smoke. */

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

    mov ax, 0x0006
    int 0x10

    mov ax, BDA_SEG
    mov es, ax
    cmp byte ptr es:[BDA_CRT_MODE], 6
    jne .fail_mode
    mov al, es:[BDA_CRT_MODE_REG]
    test al, 0x02
    jz .fail_gfx
    test al, 0x10
    jz .fail_hires

    mov ax, 0xB800
    mov es, ax
    mov byte ptr es:[0], 0xFF
    cmp byte ptr es:[0], 0xFF
    jne .fail_vram

    mov ax, 0x0003
    int 0x10

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
.fail_hires:
    push cs
    pop ds
    mov si, offset msg_hires
    call fail_and_halt
.fail_vram:
    push cs
    pop ds
    mov si, offset msg_vram
    call fail_and_halt

name:
    .asciz "bt_mode6"
msg_mode:
    .asciz "bt_mode6:mode"
msg_gfx:
    .asciz "bt_mode6:gfx"
msg_hires:
    .asciz "bt_mode6:hires"
msg_vram:
    .asciz "bt_mode6:vram"

.include "firmware/bios/tests/boot/common.inc"
