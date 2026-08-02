.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * Ctrl-Break: Ctrl held + scancode 46h → INT 1Bh (via 0x8901 inject).
 */

.equ SCAN_INJECT, 0x8901

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00

    /* Hook INT 1Bh → set flag at 0000:0505 */
    mov word ptr [0x1B * 4], offset brk_isr
    mov word ptr [0x1B * 4 + 2], cs
    mov byte ptr [0x0505], 0

    /* Set Ctrl in BDA FLAG0 */
    mov ax, 0x40
    mov ds, ax
    or byte ptr [0x17], 0x04
    and byte ptr [0x18], 0x7F
    xor ax, ax
    mov ds, ax
    sti

    /* Break/Scroll make with Ctrl */
    mov al, 0x46
    mov dx, SCAN_INJECT
    out dx, al
    hlt

    cmp byte ptr [0x0505], 1
    jne .fail_isr

    /* BDA Ctrl-Break flag (40:18 bit7) */
    mov ax, 0x40
    mov ds, ax
    test byte ptr [0x18], 0x80
    jz .fail_flag

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_isr:
    push cs
    pop ds
    mov si, offset msg_isr
    call fail_and_halt
.fail_flag:
    push cs
    pop ds
    mov si, offset msg_flag
    call fail_and_halt

brk_isr:
    push ds
    push ax
    xor ax, ax
    mov ds, ax
    mov byte ptr [0x0505], 1
    pop ax
    pop ds
    iret

name:
    .asciz "bt_brk"
msg_isr:
    .asciz "bt_brk:isr"
msg_flag:
    .asciz "bt_brk:flag"

.include "firmware/bios/tests/boot/common.inc"
