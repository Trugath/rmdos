.code16
.intel_syntax noprefix
.section .text
.global _start

/* C800: AH=09/0C/0D success; AH=15 fixed-disk DASD type */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ah, 0x00
    mov dl, 0x80
    int 0x13
    jc .fail_reset

    /* AH=09 initialize drive parameters */
    mov ah, 0x09
    mov dl, 0x80
    int 0x13
    jc .fail_09
    test ah, ah
    jnz .fail_09

    /* AH=0C seek to cyl 1 */
    mov ah, 0x0C
    mov dl, 0x80
    mov cx, 0x0101
    xor dh, dh
    int 0x13
    jc .fail_0c
    test ah, ah
    jnz .fail_0c

    /* AH=0D alternate reset */
    mov ah, 0x0D
    mov dl, 0x80
    int 0x13
    jc .fail_0d
    test ah, ah
    jnz .fail_0d

    /* AH=15 DASD type: AH=3 (fixed), CF clear, CX:DX != 0 */
    mov ah, 0x15
    mov dl, 0x80
    int 0x13
    jc .fail_15
    cmp ah, 3
    jne .fail_15
    mov ax, cx
    or ax, dx
    jz .fail_15

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_reset:
    push cs
    pop ds
    mov si, offset msg_rst
    call fail_and_halt
.fail_09:
    push cs
    pop ds
    mov si, offset msg_09
    call fail_and_halt
.fail_0c:
    push cs
    pop ds
    mov si, offset msg_0c
    call fail_and_halt
.fail_0d:
    push cs
    pop ds
    mov si, offset msg_0d
    call fail_and_halt
.fail_15:
    push cs
    pop ds
    mov si, offset msg_15
    call fail_and_halt

name:
    .asciz "bt_hd_svc"
msg_rst:
    .asciz "bt_hd_svc:rst"
msg_09:
    .asciz "bt_hd_svc:09"
msg_0c:
    .asciz "bt_hd_svc:0c"
msg_0d:
    .asciz "bt_hd_svc:0d"
msg_15:
    .asciz "bt_hd_svc:15"

.include "firmware/bios/tests/boot/common.inc"
