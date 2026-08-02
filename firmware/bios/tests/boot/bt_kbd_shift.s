.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * Shift+period → '>' via INT 09 / INT 16.
 * Inject Left Shift + period through host port 0x8901.
 */

.equ SCAN_INJECT, 0x8901

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    /* Left Shift make */
    mov al, 0x2A
    mov dx, SCAN_INJECT
    out dx, al
    hlt

    /* Period make */
    mov al, 0x34
    out dx, al
    hlt

    /* Period break */
    mov al, 0xB4
    out dx, al
    hlt

    /* Left Shift break */
    mov al, 0xAA
    out dx, al
    hlt

    mov ah, 0x00
    int 0x16
    cmp ax, 0x343E              /* scan 34h, ASCII '>' */
    jne .fail_gt

    /* Unshifted period still '.' */
    mov al, 0x34
    mov dx, SCAN_INJECT
    out dx, al
    hlt
    mov al, 0xB4
    out dx, al
    hlt

    mov ah, 0x00
    int 0x16
    cmp ax, 0x342E
    jne .fail_dot

    /* Shift+comma → '<' */
    mov al, 0x2A
    mov dx, SCAN_INJECT
    out dx, al
    hlt
    mov al, 0x33
    out dx, al
    hlt
    mov al, 0xB3
    out dx, al
    hlt
    mov al, 0xAA
    out dx, al
    hlt

    mov ah, 0x00
    int 0x16
    cmp ax, 0x333C
    jne .fail_lt

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_gt:
    push cs
    pop ds
    mov si, offset msg_gt
    call fail_and_halt
.fail_dot:
    push cs
    pop ds
    mov si, offset msg_dot
    call fail_and_halt
.fail_lt:
    push cs
    pop ds
    mov si, offset msg_lt
    call fail_and_halt

name:
    .asciz "bt_kbd_shift"
msg_gt:
    .asciz "bt_kbd_shift:>"
msg_dot:
    .asciz "bt_kbd_shift:."
msg_lt:
    .asciz "bt_kbd_shift:<"

.include "firmware/bios/tests/boot/common.inc"
