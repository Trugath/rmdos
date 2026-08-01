.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * ROM identity: F000:FFFE=FEh, date at FFF5, top-8K checksum at FE00:0000.
 */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0xF000
    mov es, ax
    cmp byte ptr es:[0xFFFE], 0xFE
    jne .fail_type

    cmp byte ptr es:[0xFFF5], '0'
    jne .fail_date
    cmp byte ptr es:[0xFFF6], '8'
    jne .fail_date
    cmp byte ptr es:[0xFFF7], '/'
    jne .fail_date

    /* Sum F000:E000 for 8K → must be 0. */
    mov si, 0xE000
    xor ax, ax
    mov cx, 8192
.sum:
    add al, es:[si]
    inc si
    loop .sum
    test al, al
    jnz .fail_sum

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_type:
    push cs
    pop ds
    mov si, offset msg_type
    call fail_and_halt
.fail_date:
    push cs
    pop ds
    mov si, offset msg_date
    call fail_and_halt
.fail_sum:
    push cs
    pop ds
    mov si, offset msg_sum
    call fail_and_halt

name:
    .asciz "bt_ident"
msg_type:
    .asciz "bt_ident:type"
msg_date:
    .asciz "bt_ident:date"
msg_sum:
    .asciz "bt_ident:sum"

.include "firmware/bios/tests/boot/common.inc"
