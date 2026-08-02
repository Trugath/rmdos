.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 15h AH=C0 configuration table */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ah, 0xC0
    int 0x15
    jc .fail_cf
    test ah, ah
    jnz .fail_ah

    mov ax, es
    test ax, ax
    jz .fail_ptr
    cmp word ptr es:[bx], 8
    jne .fail_len
    cmp byte ptr es:[bx + 2], 0xFE
    jne .fail_model

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_cf:
    push cs
    pop ds
    mov si, offset msg_cf
    call fail_and_halt
.fail_ah:
    push cs
    pop ds
    mov si, offset msg_ah
    call fail_and_halt
.fail_ptr:
    push cs
    pop ds
    mov si, offset msg_ptr
    call fail_and_halt
.fail_len:
    push cs
    pop ds
    mov si, offset msg_len
    call fail_and_halt
.fail_model:
    push cs
    pop ds
    mov si, offset msg_model
    call fail_and_halt

name:
    .asciz "bt_cfg"
msg_cf:
    .asciz "bt_cfg:cf"
msg_ah:
    .asciz "bt_cfg:ah"
msg_ptr:
    .asciz "bt_cfg:ptr"
msg_len:
    .asciz "bt_cfg:len"
msg_model:
    .asciz "bt_cfg:model"

.include "firmware/bios/tests/boot/common.inc"
