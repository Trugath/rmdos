.code16
.intel_syntax noprefix
.section .text
.global _start

/* BDA CRT mode/cols, equipment, mem size consistency with INT 11/12 */

.equ BDA_SEG, 0x0040
.equ BDA_EQUIP, 0x10
.equ BDA_MEMKB, 0x13
.equ BDA_CRT_MODE, 0x49
.equ BDA_CRT_COLS, 0x4A

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, BDA_SEG
    mov es, ax

    cmp byte ptr es:[BDA_CRT_MODE], 0x03
    jne .fail_mode
    cmp word ptr es:[BDA_CRT_COLS], 80
    jne .fail_cols

    int 0x11
    cmp ax, es:[BDA_EQUIP]
    jne .fail_equip

    int 0x12
    cmp ax, es:[BDA_MEMKB]
    jne .fail_mem
    cmp ax, 64
    jb .fail_mem

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_mode:
    push cs
    pop ds
    mov si, offset msg_mode
    call fail_and_halt
.fail_cols:
    push cs
    pop ds
    mov si, offset msg_cols
    call fail_and_halt
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
    .asciz "bt_bda"
msg_mode:
    .asciz "bt_bda:mode"
msg_cols:
    .asciz "bt_bda:cols"
msg_equip:
    .asciz "bt_bda:equip"
msg_mem:
    .asciz "bt_bda:mem"

.include "firmware/bios/tests/boot/common.inc"
