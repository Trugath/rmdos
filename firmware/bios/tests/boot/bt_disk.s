.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 13h reset, read sector 0, verify 55AA, AH=08 params */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    mov [0x7bfe], dl            /* save boot drive just below us */
    sti

    mov ah, 0x00
    mov dl, [0x7bfe]
    int 0x13
    jc .fail_reset

    /* read sector 0 into 0000:8000 */
    mov ax, 0x0201
    mov bx, 0x8000
    mov cx, 0x0001
    mov dh, 0
    mov dl, [0x7bfe]
    int 0x13
    jc .fail_read

    cmp word ptr [0x8000 + 510], 0xAA55
    jne .fail_sig

    mov ah, 0x08
    mov dl, [0x7bfe]
    int 0x13
    jc .fail_params
    /* CL low 6 bits = SPT; expect 9 for 720K test image */
    mov al, cl
    and al, 0x3F
    cmp al, 9
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
    .asciz "bt_disk"
msg_reset:
    .asciz "bt_disk:reset"
msg_read:
    .asciz "bt_disk:read"
msg_sig:
    .asciz "bt_disk:sig"
msg_params:
    .asciz "bt_disk:params"

.include "firmware/bios/tests/boot/common.inc"
