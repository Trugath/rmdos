.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 13h unsupported AH → CF + AH=01; HD unknown AH same contract */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    mov [0x7e00], dl
    sti

    /* Floppy: bogus AH=0Eh */
    mov ah, 0x0E
    mov al, 1
    mov dl, [0x7e00]
    int 0x13
    jnc .fail_fd
    cmp ah, 0x01
    jne .fail_fd

    /* HD: unsupported AH (C800 returns AH=1) */
    mov ah, 0x0E
    mov al, 1
    mov dl, 0x80
    int 0x13
    jnc .fail_hd
    cmp ah, 0x01
    jne .fail_hd

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_fd:
    push cs
    pop ds
    mov si, offset msg_fd
    call fail_and_halt
.fail_hd:
    push cs
    pop ds
    mov si, offset msg_hd
    call fail_and_halt

name:
    .asciz "bt_int13_err"
msg_fd:
    .asciz "bt_int13_err:fd"
msg_hd:
    .asciz "bt_int13_err:hd"

.include "firmware/bios/tests/boot/common.inc"
