.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * INT 13h AH=16: no change → AH=0 CF clear;
 * after OUT 0x8902 (host disk-change inject) → AH=06 CF set.
 */

.equ DCHG_INJECT, 0x8902

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    mov [0x7e00], dl
    sti

    mov ah, 0x16
    mov dl, [0x7e00]
    int 0x13
    jc .fail_clear
    test ah, ah
    jnz .fail_clear

    mov al, 1
    mov dx, DCHG_INJECT
    out dx, al

    mov ah, 0x16
    mov dl, [0x7e00]
    int 0x13
    jnc .fail_chg
    cmp ah, 0x06
    jne .fail_chg

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_clear:
    push cs
    pop ds
    mov si, offset msg_clear
    call fail_and_halt
.fail_chg:
    push cs
    pop ds
    mov si, offset msg_chg
    call fail_and_halt

name:
    .asciz "bt_chgline"
msg_clear:
    .asciz "bt_chgline:clear"
msg_chg:
    .asciz "bt_chgline:chg"

.include "firmware/bios/tests/boot/common.inc"
