.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * INT 10h AH=01 programs CRTC cursor start/end (0Ah/0Bh).
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

    mov ah, 0x01
    mov cx, 0x0607              /* start=6 end=7 (default-ish) */
    int 0x10

    mov dx, CRTC_IDX
    mov al, 0x0A
    out dx, al
    mov dx, CRTC_DATA
    in al, dx
    cmp al, 0x06
    jne .fail_start

    mov dx, CRTC_IDX
    mov al, 0x0B
    out dx, al
    mov dx, CRTC_DATA
    in al, dx
    cmp al, 0x07
    jne .fail_end

    /* Different shape */
    mov ah, 0x01
    mov cx, 0x0007
    int 0x10
    mov dx, CRTC_IDX
    mov al, 0x0A
    out dx, al
    mov dx, CRTC_DATA
    in al, dx
    cmp al, 0x00
    jne .fail_start2

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_start:
    push cs
    pop ds
    mov si, offset msg_start
    call fail_and_halt
.fail_end:
    push cs
    pop ds
    mov si, offset msg_end
    call fail_and_halt
.fail_start2:
    push cs
    pop ds
    mov si, offset msg_s2
    call fail_and_halt

name:
    .asciz "bt_ctype"
msg_start:
    .asciz "bt_ctype:start"
msg_end:
    .asciz "bt_ctype:end"
msg_s2:
    .asciz "bt_ctype:start2"

.include "firmware/bios/tests/boot/common.inc"
