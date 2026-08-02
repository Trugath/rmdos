.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 10h AH=13 write string */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0x0003
    int 0x10

    /* Place cursor elsewhere; AL=0 must leave it */
    mov ah, 0x02
    xor bh, bh
    mov dx, 0x0100
    int 0x10

    push cs
    pop es
    lea bp, [msg]
    mov cx, 3
    mov dx, 0x050A              /* row 5, col 10 */
    mov bx, 0x001F              /* page 0, attr 1F */
    mov ax, 0x1300              /* AL=0 no cursor update */
    int 0x10

    mov ah, 0x03
    xor bh, bh
    int 0x10
    cmp dx, 0x0100
    jne .fail_cur

    mov ax, 0xB800
    mov es, ax
    mov bx, (5 * 80 + 10) * 2
    cmp byte ptr es:[bx], 'S'
    jne .fail_ch
    cmp byte ptr es:[bx + 2], 'T'
    jne .fail_ch
    cmp byte ptr es:[bx + 4], 'R'
    jne .fail_ch
    cmp byte ptr es:[bx + 1], 0x1F
    jne .fail_ch

    /* AL=1 updates cursor past the string */
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
    jne .fail_upd

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_cur:
    push cs
    pop ds
    mov si, offset msg_cur
    call fail_and_halt
.fail_ch:
    push cs
    pop ds
    mov si, offset msg_ch
    call fail_and_halt
.fail_upd:
    push cs
    pop ds
    mov si, offset msg_upd
    call fail_and_halt

name:
    .asciz "bt_str"
msg:
    .ascii "STR"
msg_cur:
    .asciz "bt_str:cur"
msg_ch:
    .asciz "bt_str:ch"
msg_upd:
    .asciz "bt_str:upd"

.include "firmware/bios/tests/boot/common.inc"
