.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * INT 13h AH=00 recalibrates caller DL (0 and 1 both succeed).
 */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ah, 0x00
    mov dl, 0x00
    int 0x13
    jc .fail_d0

    /* Advertise two floppies so DL=1 is legal */
    mov ax, 0x40
    mov ds, ax
    or word ptr [0x10], 0x0040
    xor ax, ax
    mov ds, ax

    mov ah, 0x00
    mov dl, 0x01
    int 0x13
    jc .fail_d1

    /* Normal R/W still works after retry-capable path */
    mov ah, 0x00
    mov dl, 0x00
    int 0x13
    jc .fail_rw

    mov di, 0x9000
    mov cx, 512
    mov al, 0x5A
    rep stosb
    mov ax, 0x0301
    mov bx, 0x9000
    mov cx, 0x0003
    mov dx, 0x0000
    int 0x13
    jc .fail_rw

    mov di, 0x9200
    mov cx, 512
    xor al, al
    rep stosb
    mov ax, 0x0201
    mov bx, 0x9200
    mov cx, 0x0003
    mov dx, 0x0000
    int 0x13
    jc .fail_rw
    mov si, 0x9000
    mov di, 0x9200
    mov cx, 512
    repe cmpsb
    jne .fail_rw

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_d0:
    push cs
    pop ds
    mov si, offset msg_d0
    call fail_and_halt
.fail_d1:
    push cs
    pop ds
    mov si, offset msg_d1
    call fail_and_halt
.fail_rw:
    push cs
    pop ds
    mov si, offset msg_rw
    call fail_and_halt

name:
    .asciz "bt_disk_retry"
msg_d0:
    .asciz "bt_disk_retry:d0"
msg_d1:
    .asciz "bt_disk_retry:d1"
msg_rw:
    .asciz "bt_disk_retry:rw"

.include "firmware/bios/tests/boot/common.inc"
