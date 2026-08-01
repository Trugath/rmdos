.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * Shift+PrtSc via INT 09 → INT 05; status at 0000:0500 becomes FFh.
 */

.equ SCAN_INJECT, 0x8901

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    mov byte ptr [0x0500], 0
    sti

    /* Left Shift make */
    mov al, 0x2A
    mov dx, SCAN_INJECT
    out dx, al
    hlt

    /* PrtSc / keypad-* make with Shift held → INT 05 */
    mov al, 0x37
    mov dx, SCAN_INJECT
    out dx, al
    hlt

    cmp byte ptr [0x0500], 0xFF
    jne .fail_status

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_status:
    push cs
    pop ds
    mov si, offset msg_status
    call fail_and_halt

name:
    .asciz "bt_kbd_prtsc"
msg_status:
    .asciz "bt_kbd_prtsc:status"

.include "firmware/bios/tests/boot/common.inc"
