.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 10h AH=0A write char only — attribute unchanged */

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
    mov dx, 0x0202
    int 0x10

    /* Seed attr 0x2E via AH=09 */
    mov ah, 0x09
    mov al, 'X'
    mov bh, 0
    mov bl, 0x2E
    mov cx, 1
    int 0x10

    /* Overwrite char only with AH=0A */
    mov ah, 0x0A
    mov al, 'Y'
    mov bh, 0
    mov cx, 1
    int 0x10

    mov ax, 0xB800
    mov es, ax
    mov bx, (2 * 80 + 2) * 2
    cmp byte ptr es:[bx], 'Y'
    jne .fail_ch
    cmp byte ptr es:[bx + 1], 0x2E
    jne .fail_attr

    /* AH=09 CX=4 fills four cells from cursor */
    mov ah, 0x02
    xor bh, bh
    mov dx, 0x0500
    int 0x10
    mov ah, 0x09
    mov al, 'Z'
    mov bl, 0x1F
    mov cx, 4
    int 0x10
    mov bx, (5 * 80) * 2
    cmp word ptr es:[bx], 0x1F5A
    jne .fail_cx
    cmp word ptr es:[bx + 6], 0x1F5A
    jne .fail_cx
    cmp word ptr es:[bx + 8], 0x0720
    jne .fail_cx

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_ch:
    push cs
    pop ds
    mov si, offset msg_ch
    call fail_and_halt
.fail_attr:
    push cs
    pop ds
    mov si, offset msg_attr
    call fail_and_halt
.fail_cx:
    push cs
    pop ds
    mov si, offset msg_cx
    call fail_and_halt

name:
    .asciz "bt_writech"
msg_ch:
    .asciz "bt_writech:ch"
msg_attr:
    .asciz "bt_writech:attr"
msg_cx:
    .asciz "bt_writech:cx"

.include "firmware/bios/tests/boot/common.inc"
