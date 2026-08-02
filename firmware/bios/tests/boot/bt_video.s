.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 10h AH=00/02/03/0E/0F text basics */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0x0003
    int 0x10

    mov ah, 0x0F
    int 0x10
    cmp al, 0x03
    jne .fail_mode
    cmp ah, 80
    jne .fail_cols

    mov ah, 0x02
    xor bh, bh
    mov dx, 0x050A              /* row 5, col 10 */
    int 0x10

    mov ah, 0x03
    xor bh, bh
    int 0x10
    cmp dx, 0x050A
    jne .fail_cursor

    mov ah, 0x0E
    mov al, 'V'
    mov bh, 0
    int 0x10

    mov ax, 0xB800
    mov es, ax
    /* cursor was 5,10; teletype wrote there then advanced */
    mov bx, (5 * 80 + 10) * 2
    cmp byte ptr es:[bx], 'V'
    jne .fail_tty

    /* AH=04 light pen: AH=0 when absent / not triggered */
    mov ah, 0x04
    int 0x10
    cmp ah, 0
    jne .fail_pen

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_mode:
    push cs
    pop ds
    mov si, offset msg_mode
    call fail_and_halt
.fail_cols:
    push cs
    pop ds
    mov si, offset msg_cols
    call fail_and_halt
.fail_cursor:
    push cs
    pop ds
    mov si, offset msg_cursor
    call fail_and_halt
.fail_tty:
    push cs
    pop ds
    mov si, offset msg_tty
    call fail_and_halt
.fail_pen:
    push cs
    pop ds
    mov si, offset msg_pen
    call fail_and_halt

name:
    .asciz "bt_video"
msg_mode:
    .asciz "bt_video:mode"
msg_cols:
    .asciz "bt_video:cols"
msg_cursor:
    .asciz "bt_video:cursor"
msg_tty:
    .asciz "bt_video:tty"
msg_pen:
    .asciz "bt_video:pen"

.include "firmware/bios/tests/boot/common.inc"
