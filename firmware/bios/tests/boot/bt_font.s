.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * ROM font: control glyphs non-blank, lowercase ≠ uppercase, INT 1Fh high ASCII.
 */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    /* INT 1Fh must point into F000 */
    mov ax, [0x1F * 4 + 2]
    cmp ax, 0xF000
    jne .fail_ivt
    mov si, [0x1F * 4]
    /* Glyph 80h first row is 0xAA */
    push es
    mov ax, 0xF000
    mov es, ax
    cmp byte ptr es:[si], 0xAA
    pop es
    jne .fail_hi

    /* Control 01h non-blank at FA6E+8 */
    mov ax, 0xF000
    mov es, ax
    cmp byte ptr es:[0xFA6E + 8], 0
    je .fail_ctl

    /* 'A' vs 'a' bitmaps differ */
    mov si, 0xFA6E + (0x41 * 8)
    mov di, 0xFA6E + (0x61 * 8)
    mov cx, 8
.cmp_lc:
    mov al, es:[si]
    cmp al, es:[di]
    jne .lc_diff
    inc si
    inc di
    loop .cmp_lc
    jmp .fail_lc
.lc_diff:

    /* Plot high 0x80 via teletype and check a set pixel */
    mov ax, 0x0004
    int 0x10
    mov ah, 0x02
    xor bh, bh
    xor dx, dx
    int 0x10
    mov ah, 0x0E
    mov al, 0x80
    mov bh, 0
    mov bl, 0x03
    int 0x10
    mov ah, 0x0D
    xor bh, bh
    xor cx, cx
    xor dx, dx
    int 0x10
    test al, al
    jz .fail_plot

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_ivt:
    push cs
    pop ds
    mov si, offset msg_ivt
    call fail_and_halt
.fail_hi:
    push cs
    pop ds
    mov si, offset msg_hi
    call fail_and_halt
.fail_ctl:
    push cs
    pop ds
    mov si, offset msg_ctl
    call fail_and_halt
.fail_lc:
    push cs
    pop ds
    mov si, offset msg_lc
    call fail_and_halt
.fail_plot:
    push cs
    pop ds
    mov si, offset msg_plot
    call fail_and_halt

name:
    .asciz "bt_font"
msg_ivt:
    .asciz "bt_font:ivt"
msg_hi:
    .asciz "bt_font:hi"
msg_ctl:
    .asciz "bt_font:ctl"
msg_lc:
    .asciz "bt_font:lc"
msg_plot:
    .asciz "bt_font:plot"

.include "firmware/bios/tests/boot/common.inc"
