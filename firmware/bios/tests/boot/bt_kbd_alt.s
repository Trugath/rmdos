.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * Alt+letter → AL=0 (scan preserved); Ctrl+NumLock pause; AH=12 FLAG1.
 */

.equ SCAN_INJECT, 0x8901

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    /* Alt make, 'A' make/break, Alt break → AX = 1E00h */
    mov al, 0x38
    mov dx, SCAN_INJECT
    out dx, al
    hlt
    mov al, 0x1E
    out dx, al
    hlt
    mov al, 0x9E
    out dx, al
    hlt
    mov al, 0xB8
    out dx, al
    hlt

    mov ah, 0x00
    int 0x16
    cmp ax, 0x1E00
    jne .fail_alt

    /* Ctrl held → FLAG1 bit0 via AH=12 */
    mov al, 0x1D
    mov dx, SCAN_INJECT
    out dx, al
    hlt
    mov ah, 0x12
    int 0x16
    test ah, 0x01
    jz .fail_flag1
    test al, 0x04
    jz .fail_flag1

    /* Ctrl+NumLock → pause (FLAG1 bit3); no NumLock toggle */
    mov ax, 0x40
    mov ds, ax
    and byte ptr [0x17], 0xDF        /* clear NumLock */
    xor ax, ax
    mov ds, ax
    mov al, 0x45
    mov dx, SCAN_INJECT
    out dx, al
    hlt
    mov ah, 0x12
    int 0x16
    test ah, 0x08
    jz .fail_pause
    test al, 0x20
    jnz .fail_pause                 /* NumLock must not toggle */

    /* Next make ends pause and is swallowed */
    mov al, 0x1E
    out dx, al
    hlt
    mov al, 0x9E
    out dx, al
    hlt
    mov ah, 0x12
    int 0x16
    test ah, 0x08
    jnz .fail_unpause
    mov ah, 0x01
    int 0x16
    jnz .fail_swallow               /* buffer must stay empty */

    /* Ctrl break */
    mov al, 0x9D
    out dx, al
    hlt

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_alt:
    push cs
    pop ds
    mov si, offset msg_alt
    call fail_and_halt
.fail_flag1:
    push cs
    pop ds
    mov si, offset msg_flag1
    call fail_and_halt
.fail_pause:
    push cs
    pop ds
    mov si, offset msg_pause
    call fail_and_halt
.fail_unpause:
    push cs
    pop ds
    mov si, offset msg_unpause
    call fail_and_halt
.fail_swallow:
    push cs
    pop ds
    mov si, offset msg_swallow
    call fail_and_halt

name:
    .asciz "bt_kbd_alt"
msg_alt:
    .asciz "bt_kbd_alt:alt"
msg_flag1:
    .asciz "bt_kbd_alt:flag1"
msg_pause:
    .asciz "bt_kbd_alt:pause"
msg_unpause:
    .asciz "bt_kbd_alt:unpause"
msg_swallow:
    .asciz "bt_kbd_alt:swallow"

.include "firmware/bios/tests/boot/common.inc"
