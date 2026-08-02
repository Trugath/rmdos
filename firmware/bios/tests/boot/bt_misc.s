.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * Misc BIOS: INT 17 LPT1 success + DX≠0 timeout, INT 14 bad DX, INT 15 edges.
 */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    /* INT 17h DX=0 — write 'A' succeeds (no timeout bit) */
    mov ah, 0
    mov al, 'A'
    xor dx, dx
    int 0x17
    test ah, 0x01
    jnz .fail_prn

    /* Status / init also succeed */
    mov ah, 2
    xor dx, dx
    int 0x17
    test ah, 0x01
    jnz .fail_prn
    mov ah, 1
    xor dx, dx
    int 0x17
    test ah, 0x01
    jnz .fail_prn

    /* DX≠0 → timeout */
    mov ah, 0
    mov al, 'B'
    mov dx, 1
    int 0x17
    test ah, 0x01
    jz .fail_prn2

    /* INT 14h DX≠0 → timeout bit */
    mov ah, 0x03
    mov dx, 1
    int 0x14
    test ah, 0x80
    jz .fail_com

    /* INT 15h AH=81 / 82 succeed */
    mov ah, 0x81
    int 0x15
    jc .fail_i15ok
    mov ah, 0x82
    int 0x15
    jc .fail_i15ok

    /* AH=83 unsupported → CF */
    mov ah, 0x83
    int 0x15
    jnc .fail_i15cf

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_prn:
    push cs
    pop ds
    mov si, offset msg_prn
    call fail_and_halt
.fail_prn2:
    push cs
    pop ds
    mov si, offset msg_prn2
    call fail_and_halt
.fail_com:
    push cs
    pop ds
    mov si, offset msg_com
    call fail_and_halt
.fail_i15ok:
    push cs
    pop ds
    mov si, offset msg_ok
    call fail_and_halt
.fail_i15cf:
    push cs
    pop ds
    mov si, offset msg_cf
    call fail_and_halt

name:
    .asciz "bt_misc"
msg_prn:
    .asciz "bt_misc:prn"
msg_prn2:
    .asciz "bt_misc:prn2"
msg_com:
    .asciz "bt_misc:com"
msg_ok:
    .asciz "bt_misc:i15ok"
msg_cf:
    .asciz "bt_misc:i15cf"

.include "firmware/bios/tests/boot/common.inc"
