.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 10h AH=05 selects distinct text pages and programs their start. */

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
    cmp word ptr [0x4e], 0x0800 /* 80x25 page = 0x800 character cells */
    jne .fail_start
    xor ax, ax
    mov ds, ax

    mov ah, 0x0F
    int 0x10
    cmp bh, 1
    jne .fail_get

    /* Cursor on page 1, write a different character. */
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

    mov ax, 0x0500
    int 0x10
    mov ah, 0x0F
    int 0x10
    cmp bh, 0
    jne .fail_restore
    mov ax, 0x40
    mov ds, ax
    cmp word ptr [0x4e], 0
    jne .fail_start
    xor ax, ax
    mov ds, ax

    /* AH=08 must read the requested inactive page, not visible page zero. */
    mov ah, 0x08
    mov bh, 1
    int 0x10
    cmp al, 'P'
    jne .fail_read1

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
.fail_start:
    push cs
    pop ds
    mov si, offset msg_start
    call fail_and_halt
.fail_read1:
    push cs
    pop ds
    mov si, offset msg_read1
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
msg_start:
    .asciz "bt_page:start"
msg_read1:
    .asciz "bt_page:read1"
msg_restore:
    .asciz "bt_page:restore"

.include "firmware/bios/tests/boot/common.inc"
