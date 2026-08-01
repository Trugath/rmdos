.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 13h AH=08 on a 1.2M image: expect SPT=15, drive type=2 */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    mov [0x7e00], dl
    sti

    mov ah, 0x00
    mov dl, [0x7e00]
    int 0x13
    jc .fail_reset

    mov ax, 0x0201
    mov bx, 0x8000
    mov cx, 0x0001
    mov dh, 0
    mov dl, [0x7e00]
    int 0x13
    jc .fail_read

    cmp word ptr [0x8000 + 510], 0xAA55
    jne .fail_sig

    mov ah, 0x08
    mov dl, [0x7e00]
    int 0x13
    jc .fail_params
    mov al, cl
    and al, 0x3F
    cmp al, 15
    jne .fail_params
    cmp bl, 2
    jne .fail_params

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_reset:
    push cs
    pop ds
    mov si, offset msg_reset
    call fail_and_halt
.fail_read:
    push cs
    pop ds
    mov si, offset msg_read
    call fail_and_halt
.fail_sig:
    push cs
    pop ds
    mov si, offset msg_sig
    call fail_and_halt
.fail_params:
    push cs
    pop ds
    mov si, offset msg_params
    call fail_and_halt

name:
    .asciz "bt_disk120"
msg_reset:
    .asciz "bt_disk120:reset"
msg_read:
    .asciz "bt_disk120:read"
msg_sig:
    .asciz "bt_disk120:sig"
msg_params:
    .asciz "bt_disk120:params"

.include "firmware/bios/tests/boot/common.inc"
