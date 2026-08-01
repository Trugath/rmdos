.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * Mode 4 graphics teletype scroll: pixel at y=8 moves to y=0 after LF wrap.
 */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0x0004
    int 0x10

    /* Plot (5,8) color 2 */
    mov ah, 0x0C
    mov al, 0x02
    xor bh, bh
    mov cx, 5
    mov dx, 8
    int 0x10

    /* Cursor row 24 → LF triggers gfx scroll */
    mov ah, 0x02
    xor bh, bh
    mov dx, 0x1800
    int 0x10
    mov ah, 0x0E
    mov al, 0x0A
    mov bh, 0
    mov bl, 0x02
    int 0x10

    /* (5,0) should now be former (5,8) */
    mov ah, 0x0D
    xor bh, bh
    mov cx, 5
    xor dx, dx
    int 0x10
    cmp al, 0x02
    jne .fail_moved

    /* Original (5,8) cleared (scrolled away / overwritten by next rows) —
       after one row scroll, y=8 came from y=16 which was empty → 0 */
    mov ah, 0x0D
    mov cx, 5
    mov dx, 8
    int 0x10
    cmp al, 0
    jne .fail_clear

    mov ax, 0x0003
    int 0x10

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_moved:
    push cs
    pop ds
    mov si, offset msg_moved
    call fail_and_halt
.fail_clear:
    push cs
    pop ds
    mov si, offset msg_clear
    call fail_and_halt

name:
    .asciz "bt_gfx_scroll"
msg_moved:
    .asciz "bt_gfx_scroll:moved"
msg_clear:
    .asciz "bt_gfx_scroll:clear"

.include "firmware/bios/tests/boot/common.inc"
