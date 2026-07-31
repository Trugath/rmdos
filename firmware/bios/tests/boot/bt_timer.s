.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 1Ah get ticks; wait for increase */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    xor ah, ah
    int 0x1A
    mov [0x0500], dx
    mov [0x0502], cx

    /* spin until ticks change (IRQ0 ~18.2 Hz) */
    mov bx, 0
.wait:
    xor ah, ah
    int 0x1A
    cmp dx, [0x0500]
    jne .changed
    cmp cx, [0x0502]
    jne .changed
    inc bx
    jnz .wait
    jmp .fail_stuck

.changed:
    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_stuck:
    push cs
    pop ds
    mov si, offset msg_stuck
    call fail_and_halt

name:
    .asciz "bt_timer"
msg_stuck:
    .asciz "bt_timer:stuck"

.include "firmware/bios/tests/boot/common.inc"
