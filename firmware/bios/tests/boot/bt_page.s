.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 10h AH=05 active page + AH=0F BH; AH=09 write on page 1 */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0x0003
    int 0x10

    mov ax, 0x0501
    int 0x10

    mov ax, 0x40
    mov ds, ax
    cmp byte ptr [0x62], 1
    jne .fail_bda
    xor ax, ax
    mov ds, ax

    mov ah, 0x0F
    int 0x10
    cmp bh, 1
    jne .fail_get

    /* Cursor on page 1, write 'P' */
    mov ah, 0x02
    mov bh, 1
    xor dx, dx
    int 0x10
    mov ah, 0x09
    mov al, 'P'
    mov bh, 1
    mov bl, 0x07
    mov cx, 1
    int 0x10

    mov ax, 0xB800
    mov es, ax
    cmp byte ptr es:[0], 'P'
    jne .fail_write

    mov ax, 0x0500
    int 0x10
    mov ah, 0x0F
    int 0x10
    cmp bh, 0
    jne .fail_restore

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_bda:
    push cs
    pop ds
    mov si, offset msg_bda
    call fail_and_halt
.fail_get:
    push cs
    pop ds
    mov si, offset msg_get
    call fail_and_halt
.fail_write:
    push cs
    pop ds
    mov si, offset msg_write
    call fail_and_halt
.fail_restore:
    push cs
    pop ds
    mov si, offset msg_restore
    call fail_and_halt

name:
    .asciz "bt_page"
msg_bda:
    .asciz "bt_page:bda"
msg_get:
    .asciz "bt_page:get"
msg_write:
    .asciz "bt_page:write"
msg_restore:
    .asciz "bt_page:restore"

.include "firmware/bios/tests/boot/common.inc"
