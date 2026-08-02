.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * INT 10h AH=0E text teletype: CR, BS, col wrap, LF scroll, CRTC sync.
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

    /* CR: col 5 → 0, same row */
    mov ah, 0x02
    xor bh, bh
    mov dx, 0x0305
    int 0x10
    mov ah, 0x0E
    mov al, 0x0D
    int 0x10
    mov ah, 0x03
    int 0x10
    cmp dx, 0x0300
    jne .fail_cr

    /* BS: col 4 → 3 */
    mov ah, 0x02
    mov dx, 0x0404
    int 0x10
    mov ah, 0x0E
    mov al, 0x08
    int 0x10
    mov ah, 0x03
    int 0x10
    cmp dx, 0x0403
    jne .fail_bs

    /* Wrap at col 79 → next row col 0 */
    mov ah, 0x02
    mov dx, 0x054F
    int 0x10
    mov ah, 0x0E
    mov al, 'W'
    int 0x10
    mov ah, 0x03
    int 0x10
    cmp dx, 0x0600
    jne .fail_wrap
    mov ax, 0xB800
    mov es, ax
    cmp byte ptr es:[(5 * 80 + 79) * 2], 'W'
    jne .fail_wrap

    /* LF at row 24 scrolls up: 'S' at (1,0) → (0,0) */
    mov word ptr es:[160], 0x0753
    mov ah, 0x02
    xor bh, bh
    mov dx, 0x1800
    int 0x10
    mov ah, 0x0E
    mov al, 0x0A
    int 0x10
    cmp byte ptr es:[0], 'S'
    jne .fail_lf
    cmp byte ptr es:[160], ' '
    jne .fail_lf
    mov ah, 0x03
    int 0x10
    cmp dx, 0x1800                    /* stays row 24 col 0 */
    jne .fail_lf

    /* CRTC after printable: write at (2,3) → cursor (2,4) → addr 164 */
    mov ah, 0x02
    mov dx, 0x0203
    int 0x10
    mov ah, 0x0E
    mov al, 'C'
    int 0x10
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
    cmp ax, (2 * 80 + 4)
    jne .fail_crtc

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

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
.fail_crtc:
    push cs
    pop ds
    mov si, offset msg_crtc
    call fail_and_halt

name:
    .asciz "bt_tty"
msg_cr:
    .asciz "bt_tty:cr"
msg_bs:
    .asciz "bt_tty:bs"
msg_wrap:
    .asciz "bt_tty:wrap"
msg_lf:
    .asciz "bt_tty:lf"
msg_crtc:
    .asciz "bt_tty:crtc"

.include "firmware/bios/tests/boot/common.inc"
