.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 15h: AH=80 ok, unknown CF, AH=86 advances ticks */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ah, 0x80
    int 0x15
    jc .fail_ok

    mov ah, 0xFF
    int 0x15
    jnc .fail_unk

    xor ah, ah
    int 0x1A
    mov [0x0500], dx
    mov [0x0502], cx

    /* Wait ~110ms (2 ticks): CX:DX = 110000 µs */
    mov ah, 0x86
    mov cx, 0x0001
    mov dx, 0xADB0              /* 0x1ADB0 = 110000 */
    int 0x15
    jc .fail_wait

    xor ah, ah
    int 0x1A
    cmp dx, [0x0500]
    jne .changed
    cmp cx, [0x0502]
    jne .changed
    jmp .fail_stuck

.changed:
    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_ok:
    push cs
    pop ds
    mov si, offset msg_ok
    call fail_and_halt
.fail_unk:
    push cs
    pop ds
    mov si, offset msg_unk
    call fail_and_halt
.fail_wait:
    push cs
    pop ds
    mov si, offset msg_wait
    call fail_and_halt
.fail_stuck:
    push cs
    pop ds
    mov si, offset msg_stuck
    call fail_and_halt

name:
    .asciz "bt_int15"
msg_ok:
    .asciz "bt_int15:ah80"
msg_unk:
    .asciz "bt_int15:unk"
msg_wait:
    .asciz "bt_int15:waitcf"
msg_stuck:
    .asciz "bt_int15:stuck"

.include "firmware/bios/tests/boot/common.inc"
