.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 10h AH=13 write string AL=0/1 and AL=2/3 */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0x0003
    int 0x10

    mov ah, 0x02
    xor bh, bh
    mov dx, 0x0100
    int 0x10

    push cs
    pop es
    lea bp, [msg]
    mov cx, 3
    mov dx, 0x050A
    mov bx, 0x001F
    mov ax, 0x1300
    int 0x10

    mov ah, 0x03
    xor bh, bh
    int 0x10
    cmp dx, 0x0100
    jne .fail

    mov ax, 0xB800
    mov es, ax
    mov bx, (5 * 80 + 10) * 2
    cmp word ptr es:[bx], 0x1F53        /* 'S' + attr 1F */
    jne .fail
    cmp byte ptr es:[bx + 2], 'T'
    jne .fail

    push cs
    pop es
    lea bp, [msg]
    mov cx, 3
    mov dx, 0x0600
    mov bx, 0x0007
    mov ax, 0x1301
    int 0x10
    mov ah, 0x03
    xor bh, bh
    int 0x10
    cmp dx, 0x0603
    jne .fail

    /* AL=2 char+attr pairs */
    mov ah, 0x02
    xor bh, bh
    mov dx, 0x0200
    int 0x10
    push cs
    pop es
    lea bp, [msg_pair]
    mov cx, 2
    mov dx, 0x0705
    xor bx, bx
    mov ax, 0x1302
    int 0x10
    mov ah, 0x03
    xor bh, bh
    int 0x10
    cmp dx, 0x0200
    jne .fail
    mov ax, 0xB800
    mov es, ax
    mov bx, (7 * 80 + 5) * 2
    cmp word ptr es:[bx], 0x2E50        /* 'P', 0x2E */
    jne .fail
    cmp word ptr es:[bx + 2], 0x4F51    /* 'Q', 0x4F */
    jne .fail

    /* AL=3 updates cursor */
    push cs
    pop es
    lea bp, [msg_pair]
    mov cx, 2
    mov dx, 0x0800
    xor bx, bx
    mov ax, 0x1303
    int 0x10
    mov ah, 0x03
    xor bh, bh
    int 0x10
    cmp dx, 0x0802
    jne .fail

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail:
    push cs
    pop ds
    mov si, offset name
    call fail_and_halt

name:
    .asciz "bt_str"
msg:
    .ascii "STR"
msg_pair:
    .byte 'P', 0x2E, 'Q', 0x4F

.include "firmware/bios/tests/boot/common.inc"
