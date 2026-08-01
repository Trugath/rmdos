.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * Mode 4 pixel AH=0C/0D round-trip + CRTC cursor after AH=02 in text mode.
 */

.equ BDA_SEG, 0x0040
.equ CRTC_IDX, 0x3D4
.equ CRTC_DATA, 0x3D5

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0x0004
    int 0x10

    /* Write pixel (10,20) color 2 */
    mov ah, 0x0C
    mov al, 0x02
    xor bh, bh
    mov cx, 10
    mov dx, 20
    int 0x10

    /* Read back */
    mov ah, 0x0D
    xor bh, bh
    mov cx, 10
    mov dx, 20
    int 0x10
    cmp al, 0x02
    jne .fail_pix

    /* Text mode + CRTC cursor */
    mov ax, 0x0003
    int 0x10
    mov ah, 0x02
    xor bh, bh
    mov dx, 0x0102              /* row 1, col 2 → offset 82 */
    int 0x10

    mov dx, CRTC_IDX
    mov al, 0x0E
    out dx, al
    mov dx, CRTC_DATA
    in al, dx
    mov ah, al                   /* high */
    mov dx, CRTC_IDX
    mov al, 0x0F
    out dx, al
    mov dx, CRTC_DATA
    in al, dx                    /* AX = cursor address */
    cmp ax, 82
    jne .fail_crtc

    /* BEL smoke */
    mov ah, 0x0E
    mov al, 0x07
    mov bh, 0
    int 0x10

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_pix:
    push cs
    pop ds
    mov si, offset msg_pix
    call fail_and_halt
.fail_crtc:
    push cs
    pop ds
    mov si, offset msg_crtc
    call fail_and_halt

name:
    .asciz "bt_pixel"
msg_pix:
    .asciz "bt_pixel:pix"
msg_crtc:
    .asciz "bt_pixel:crtc"

.include "firmware/bios/tests/boot/common.inc"
