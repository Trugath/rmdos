.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 16h AH=05: fill buffer then CF; AH=01 still reports data */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    /* Drain any pending keys */
.drain:
    mov ah, 0x01
    int 0x16
    jz .empty
    xor ah, ah
    int 0x16
    jmp .drain
.empty:

    /* Stuff until full (16-slot ring → 15 usable) */
    xor bx, bx
.fill:
    mov ah, 0x05
    mov cx, 0x1E41              /* 'A' */
    int 0x16
    jc .filled
    inc bx
    cmp bx, 20
    jb .fill
    jmp .fail_nofull

.filled:
    test bx, bx
    jz .fail_nofull

    /* One more stuff must still CF */
    mov ah, 0x05
    mov cx, 0x1E42
    int 0x16
    jnc .fail_still

    /* Buffer not empty */
    mov ah, 0x01
    int 0x16
    jz .fail_stat

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_nofull:
    push cs
    pop ds
    mov si, offset msg_nf
    call fail_and_halt
.fail_still:
    push cs
    pop ds
    mov si, offset msg_st
    call fail_and_halt
.fail_stat:
    push cs
    pop ds
    mov si, offset msg_av
    call fail_and_halt

name:
    .asciz "bt_kbd_full"
msg_nf:
    .asciz "bt_kbd_full:nf"
msg_st:
    .asciz "bt_kbd_full:st"
msg_av:
    .asciz "bt_kbd_full:av"

.include "firmware/bios/tests/boot/common.inc"
