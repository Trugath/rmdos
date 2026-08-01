.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * Caps Lock via INT 09: inject scancode through host port 0x8901,
 * then AH=02 must show Caps flag (40:17 bit 6).
 */

.equ SCAN_INJECT, 0x8901

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00

    /* Clear Caps in FLAG0 */
    mov ax, 0x40
    mov ds, ax
    and byte ptr [0x17], 0xBF
    xor ax, ax
    mov ds, ax
    sti

    /* Caps Lock make */
    mov al, 0x3A
    mov dx, SCAN_INJECT
    out dx, al
    hlt

    mov ah, 0x02
    int 0x16
    test al, 0x40
    jz .fail_caps

    /* Caps break should not clear the toggle */
    mov al, 0xBA
    mov dx, SCAN_INJECT
    out dx, al
    hlt

    mov ah, 0x02
    int 0x16
    test al, 0x40
    jz .fail_hold

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_caps:
    push cs
    pop ds
    mov si, offset msg_caps
    call fail_and_halt
.fail_hold:
    push cs
    pop ds
    mov si, offset msg_hold
    call fail_and_halt

name:
    .asciz "bt_kbd_irq"
msg_caps:
    .asciz "bt_kbd_irq:caps"
msg_hold:
    .asciz "bt_kbd_irq:hold"

.include "firmware/bios/tests/boot/common.inc"
