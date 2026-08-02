.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * INT 10h AH=0E: CRTC after CR/BS/wrap/LF; BEL leaves CRTC unchanged.
 */

.equ CRTC_IDX, 0x3D4
.equ CRTC_DATA, 0x3D5

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0x0003
    int 0x10

    /* CR → CRTC 3*80 */
    mov ah, 0x02
    xor bh, bh
    mov dx, 0x0305
    int 0x10
    mov ah, 0x0E
    mov al, 0x0D
    int 0x10
    call read_crtc
    cmp ax, (3 * 80)
    jne .fail_cr

    /* BS → CRTC 4*80+3 */
    mov ah, 0x02
    mov dx, 0x0404
    int 0x10
    mov ah, 0x0E
    mov al, 0x08
    int 0x10
    call read_crtc
    cmp ax, (4 * 80 + 3)
    jne .fail_bs

    /* Wrap → CRTC 6*80 */
    mov ah, 0x02
    mov dx, 0x054F
    int 0x10
    mov ah, 0x0E
    mov al, 'W'
    int 0x10
    call read_crtc
    cmp ax, (6 * 80)
    jne .fail_wrap

    /* LF at bottom → CRTC 24*80 */
    mov ah, 0x02
    mov dx, 0x1800
    int 0x10
    mov ah, 0x0E
    mov al, 0x0A
    int 0x10
    call read_crtc
    cmp ax, (24 * 80)
    jne .fail_lf

    /* BEL leaves CRTC alone */
    mov ah, 0x02
    mov dx, 0x0101
    int 0x10
    call read_crtc
    mov bx, ax
    mov ah, 0x0E
    mov al, 0x07
    int 0x10
    call read_crtc
    cmp ax, bx
    jne .fail_bel

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

read_crtc:
    push dx
    mov dx, CRTC_IDX
    mov al, 0x0E
    out dx, al
    mov dx, CRTC_DATA
    in al, dx
    mov ah, al
    mov dx, CRTC_IDX
    mov al, 0x0F
    out dx, al
    mov dx, CRTC_DATA
    in al, dx
    pop dx
    ret

.fail_cr:
    push cs
    pop ds
    mov si, offset msg_cr
    call fail_and_halt
.fail_bs:
    push cs
    pop ds
    mov si, offset msg_bs
    call fail_and_halt
.fail_wrap:
    push cs
    pop ds
    mov si, offset msg_wrap
    call fail_and_halt
.fail_lf:
    push cs
    pop ds
    mov si, offset msg_lf
    call fail_and_halt
.fail_bel:
    push cs
    pop ds
    mov si, offset msg_bel
    call fail_and_halt

name:
    .asciz "bt_tty2"
msg_cr:
    .asciz "bt_tty2:cr"
msg_bs:
    .asciz "bt_tty2:bs"
msg_wrap:
    .asciz "bt_tty2:wrap"
msg_lf:
    .asciz "bt_tty2:lf"
msg_bel:
    .asciz "bt_tty2:bel"

.include "firmware/bios/tests/boot/common.inc"
