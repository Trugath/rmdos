.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * On a 360K image, AH=08 → type 1; AH=02 at CH>=40 upgrades to 720K;
 * subsequent AH=08 → type 3 (BL=3).
 */

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

    mov ah, 0x08
    mov dl, [0x7e00]
    int 0x13
    jc .fail_params
    cmp bl, 1
    jne .fail_params

    /* Read cyl 40 — may fail on short image; upgrade still runs after store_status. */
    mov ax, 0x0201
    mov bx, 0x8000
    mov cx, 0x2801
    mov dh, 0
    mov dl, [0x7e00]
    int 0x13
    /* CF ignored */

    mov ah, 0x08
    mov dl, [0x7e00]
    int 0x13
    jc .fail_up
    cmp bl, 3
    jne .fail_up

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_reset:
    push cs
    pop ds
    mov si, offset msg_reset
    call fail_and_halt
.fail_params:
    push cs
    pop ds
    mov si, offset msg_params
    call fail_and_halt
.fail_up:
    push cs
    pop ds
    mov si, offset msg_up
    call fail_and_halt

name:
    .asciz "bt_disk_upgrade"
msg_reset:
    .asciz "bt_disk_upgrade:reset"
msg_params:
    .asciz "bt_disk_upgrade:params"
msg_up:
    .asciz "bt_disk_upgrade:up"

.include "firmware/bios/tests/boot/common.inc"
