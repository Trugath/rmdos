.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * IBM absolute trampolines: opcode check + INT service smoke.
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

    /* Each trampoline starts with near JMP (E9). */
    cmp byte ptr es:[0xE6F2], 0xE9
    jne .fail_op
    cmp byte ptr es:[0xE82E], 0xE9
    jne .fail_op
    cmp byte ptr es:[0xF84D], 0xE9
    jne .fail_op
    cmp byte ptr es:[0xFE6E], 0xE9
    jne .fail_op
    cmp byte ptr es:[0xFF54], 0xE9
    jne .fail_op

    /* Baud table @ E729: 110 baud divisor = 1047. */
    cmp word ptr es:[0xE729], 1047
    jne .fail_baud

    /* Functional: INT 11 via normal IVT (same handler as F84D trampoline). */
    int 0x11
    test ax, ax
    jz .fail_eq

    /* INT 1Ah AH=0 */
    xor ah, ah
    int 0x1A

    /* INT 16h AH=1 empty */
    mov ah, 0x01
    int 0x16
    jnz .fail_kbd

    /* INT 5 → status FFh with stub printer */
    mov byte ptr [0x0500], 0
    int 0x05
    cmp byte ptr [0x0500], 0xFF
    jne .fail_prtsc

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_op:
    push cs
    pop ds
    mov si, offset msg_op
    call fail_and_halt
.fail_baud:
    push cs
    pop ds
    mov si, offset msg_baud
    call fail_and_halt
.fail_eq:
    push cs
    pop ds
    mov si, offset msg_eq
    call fail_and_halt
.fail_kbd:
    push cs
    pop ds
    mov si, offset msg_kbd
    call fail_and_halt
.fail_prtsc:
    push cs
    pop ds
    mov si, offset msg_prtsc
    call fail_and_halt

name:
    .asciz "bt_entry"
msg_op:
    .asciz "bt_entry:op"
msg_baud:
    .asciz "bt_entry:baud"
msg_eq:
    .asciz "bt_entry:eq"
msg_kbd:
    .asciz "bt_entry:kbd"
msg_prtsc:
    .asciz "bt_entry:prtsc"

.include "firmware/bios/tests/boot/common.inc"
