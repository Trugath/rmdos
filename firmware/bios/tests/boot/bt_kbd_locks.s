.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * Num Lock / Scroll Lock / Insert toggles via INT 09 (port 0x8901).
 * Break codes must not clear the toggles.
 */

.equ SCAN_INJECT, 0x8901

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00

    /* Clear Num/Scroll/Insert; ensure Ctrl clear (Scroll vs Break) */
    mov ax, 0x40
    mov ds, ax
    and byte ptr [0x17], 0x0F
    xor ax, ax
    mov ds, ax
    sti

    /* Num Lock make 45h → bit5 */
    mov al, 0x45
    mov dx, SCAN_INJECT
    out dx, al
    hlt

    mov ah, 0x02
    int 0x16
    test al, 0x20
    jz .fail_num

    mov al, 0xC5
    mov dx, SCAN_INJECT
    out dx, al
    hlt

    mov ah, 0x02
    int 0x16
    test al, 0x20
    jz .fail_numh

    /* Scroll Lock make 46h (no Ctrl) → bit4 */
    mov ax, 0x40
    mov ds, ax
    and byte ptr [0x17], 0xFB
    xor ax, ax
    mov ds, ax

    mov al, 0x46
    mov dx, SCAN_INJECT
    out dx, al
    hlt

    mov ah, 0x02
    int 0x16
    test al, 0x10
    jz .fail_scr

    mov al, 0xC6
    mov dx, SCAN_INJECT
    out dx, al
    hlt

    mov ah, 0x02
    int 0x16
    test al, 0x10
    jz .fail_scrh

    /* Insert: NumLock off + make 52h → bit7 */
    mov ax, 0x40
    mov ds, ax
    and byte ptr [0x17], 0x5F        /* clear Num + Insert */
    xor ax, ax
    mov ds, ax

    mov al, 0x52
    mov dx, SCAN_INJECT
    out dx, al
    hlt

    mov ah, 0x02
    int 0x16
    test al, 0x80
    jz .fail_ins

    mov al, 0xD2
    mov dx, SCAN_INJECT
    out dx, al
    hlt

    mov ah, 0x02
    int 0x16
    test al, 0x80
    jz .fail_insh

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_num:
    push cs
    pop ds
    mov si, offset msg_num
    call fail_and_halt
.fail_numh:
    push cs
    pop ds
    mov si, offset msg_numh
    call fail_and_halt
.fail_scr:
    push cs
    pop ds
    mov si, offset msg_scr
    call fail_and_halt
.fail_scrh:
    push cs
    pop ds
    mov si, offset msg_scrh
    call fail_and_halt
.fail_ins:
    push cs
    pop ds
    mov si, offset msg_ins
    call fail_and_halt
.fail_insh:
    push cs
    pop ds
    mov si, offset msg_insh
    call fail_and_halt

name:
    .asciz "bt_kbd_locks"
msg_num:
    .asciz "bt_kbd_locks:num"
msg_numh:
    .asciz "bt_kbd_locks:numh"
msg_scr:
    .asciz "bt_kbd_locks:scr"
msg_scrh:
    .asciz "bt_kbd_locks:scrh"
msg_ins:
    .asciz "bt_kbd_locks:ins"
msg_insh:
    .asciz "bt_kbd_locks:insh"

.include "firmware/bios/tests/boot/common.inc"
