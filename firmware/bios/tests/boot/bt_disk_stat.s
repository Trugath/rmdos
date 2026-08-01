.code16
.intel_syntax noprefix
.section .text
.global _start

/* AH=01 returns BDA floppy status: clear after reset; CF+AH after forced error */

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

    mov ah, 0x01
    mov dl, [0x7e00]
    int 0x13
    jc .fail_clear
    test ah, ah
    jnz .fail_clear

    /* Force last-status error in BDA 40:41 (offset 0x40 from 40:00). */
    mov ax, 0x40
    mov ds, ax
    mov byte ptr [0x40], 0x80
    xor ax, ax
    mov ds, ax

    mov ah, 0x01
    mov dl, [0x7e00]
    int 0x13
    jnc .fail_status
    cmp ah, 0x80
    jne .fail_status

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_reset:
    push cs
    pop ds
    mov si, offset msg_reset
    call fail_and_halt
.fail_clear:
    push cs
    pop ds
    mov si, offset msg_clear
    call fail_and_halt
.fail_status:
    push cs
    pop ds
    mov si, offset msg_status
    call fail_and_halt

name:
    .asciz "bt_disk_stat"
msg_reset:
    .asciz "bt_disk_stat:reset"
msg_clear:
    .asciz "bt_disk_stat:clear"
msg_status:
    .asciz "bt_disk_stat:status"

.include "firmware/bios/tests/boot/common.inc"
