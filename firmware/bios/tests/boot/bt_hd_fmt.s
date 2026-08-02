.code16
.intel_syntax noprefix
.section .text
.global _start

/* C800: AH=05 format cyl 1 head 0, then AH=04 verify sec 1 */

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

    mov ah, 0x05
    mov al, 17
    xor bx, bx
    mov cx, 0x0101              /* cyl 1 */
    xor dx, dx
    mov dl, 0x80                /* head 0, drive 80 */
    int 0x13
    jc .fail_fmt
    test ah, ah
    jnz .fail_fmt

    mov ax, 0x0401
    mov bx, 0x9000
    mov cx, 0x0101
    xor dx, dx
    mov dl, 0x80
    int 0x13
    jc .fail_verify
    test ah, ah
    jnz .fail_verify

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_reset:
    push cs
    pop ds
    mov si, offset msg_rst
    call fail_and_halt
.fail_fmt:
    push cs
    pop ds
    mov si, offset msg_fmt
    call fail_and_halt
.fail_verify:
    push cs
    pop ds
    mov si, offset msg_vf
    call fail_and_halt

name:
    .asciz "bt_hd_fmt"
msg_rst:
    .asciz "bt_hd_fmt:rst"
msg_fmt:
    .asciz "bt_hd_fmt:fmt"
msg_vf:
    .asciz "bt_hd_fmt:vf"

.include "firmware/bios/tests/boot/common.inc"
