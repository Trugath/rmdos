.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * INT 14h COM1: init, status (THRE), 8250 loopback TX/RX round-trip.
 */

.equ COM1_MCR, 0x3FC

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    /* AH=00 init: 9600 8N1 (AL bits 7-5=111, word=11) */
    mov ah, 0x00
    mov al, 0xE3
    xor dx, dx
    int 0x14
    test ah, 0x80
    jnz .fail_init

    /* AH=03 status — THRE should be set */
    mov ah, 0x03
    xor dx, dx
    int 0x14
    test ah, 0x20
    jz .fail_status

    /* Enable 8250 loopback */
    mov dx, COM1_MCR
    in al, dx
    or al, 0x10
    out dx, al

    /* Send 'Q' via INT 14h */
    mov ah, 0x01
    mov al, 'Q'
    xor dx, dx
    int 0x14
    test ah, 0x80
    jnz .fail_send

    /* Receive via INT 14h */
    mov ah, 0x02
    xor dx, dx
    int 0x14
    test ah, 0x80
    jnz .fail_recv
    cmp al, 'Q'
    jne .fail_mismatch

    /* Disable loopback */
    mov dx, COM1_MCR
    in al, dx
    and al, 0xEF
    out dx, al

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_init:
    push cs
    pop ds
    mov si, offset msg_init
    call fail_and_halt
.fail_status:
    push cs
    pop ds
    mov si, offset msg_status
    call fail_and_halt
.fail_send:
    push cs
    pop ds
    mov si, offset msg_send
    call fail_and_halt
.fail_recv:
    push cs
    pop ds
    mov si, offset msg_recv
    call fail_and_halt
.fail_mismatch:
    push cs
    pop ds
    mov si, offset msg_mis
    call fail_and_halt

name:
    .asciz "bt_serial"
msg_init:
    .asciz "bt_serial:init"
msg_status:
    .asciz "bt_serial:status"
msg_send:
    .asciz "bt_serial:send"
msg_recv:
    .asciz "bt_serial:recv"
msg_mis:
    .asciz "bt_serial:mismatch"

.include "firmware/bios/tests/boot/common.inc"
