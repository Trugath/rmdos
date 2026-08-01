.code16
.intel_syntax noprefix
.section .text
.global _start

/* Mode 4 + INT 10h AH=0B palette → BDA 40:66 and port 3D9 */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0x0004
    int 0x10

    /* BH=0 set background/border from BL */
    mov ah, 0x0B
    xor bh, bh
    mov bl, 0x01
    int 0x10

    mov ax, 0x40
    mov ds, ax
    mov al, [0x66]
    and al, 0x1F
    cmp al, 0x01
    jne .fail_bg

    /* BH=1 select palette 1 */
    xor ax, ax
    mov ds, ax
    mov ah, 0x0B
    mov bh, 1
    mov bl, 1
    int 0x10

    mov ax, 0x40
    mov ds, ax
    test byte ptr [0x66], 0x20
    jz .fail_pal

    xor ax, ax
    mov ds, ax
    mov ax, 0x0003
    int 0x10

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_bg:
    push cs
    pop ds
    mov si, offset msg_bg
    call fail_and_halt
.fail_pal:
    push cs
    pop ds
    mov si, offset msg_pal
    call fail_and_halt

name:
    .asciz "bt_palette"
msg_bg:
    .asciz "bt_palette:bg"
msg_pal:
    .asciz "bt_palette:pal"

.include "firmware/bios/tests/boot/common.inc"
