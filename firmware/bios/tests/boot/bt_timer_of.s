.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 1Ah midnight overflow: force near-max ticks, AH=00 returns AL=1 */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    /* Next IRQ0 will roll past 0x1800B0 */
    mov ah, 0x01
    mov cx, 0x0018
    mov dx, 0x00AF
    int 0x1A

    /* Wait for overflow flag (BDA 40:70) */
    mov ax, 0x40
    mov es, ax
    mov cx, 0x8000
.wait:
    cmp byte ptr es:[0x70], 0
    jne .got
    loop .wait
    jmp .fail_wait

.got:
    xor ah, ah
    int 0x1A
    cmp al, 1
    jne .fail_al
    /* Flag cleared on read */
    cmp byte ptr es:[0x70], 0
    jne .fail_clr

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_wait:
    push cs
    pop ds
    mov si, offset msg_wait
    call fail_and_halt
.fail_al:
    push cs
    pop ds
    mov si, offset msg_al
    call fail_and_halt
.fail_clr:
    push cs
    pop ds
    mov si, offset msg_clr
    call fail_and_halt

name:
    .asciz "bt_timer_of"
msg_wait:
    .asciz "bt_timer_of:wait"
msg_al:
    .asciz "bt_timer_of:al"
msg_clr:
    .asciz "bt_timer_of:clr"

.include "firmware/bios/tests/boot/common.inc"
