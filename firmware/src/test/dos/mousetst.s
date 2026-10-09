.code16
.intel_syntax noprefix
.section .text
.global _start

.set INJECT, 0x8903

_start:
    xor ax, ax
    int 0x33
    cmp ax, 0xFFFF
    jne .fail_nodrv

    mov ax, 0x0004
    mov cx, 100
    mov dx, 50
    int 0x33

    cli
    mov dx, INJECT
    mov al, 0x01
    out dx, al
    mov al, 10
    out dx, al
    mov al, 5
    out dx, al

    mov ax, 0x0003
    int 0x33
    sti

    /* inject dx=+10, dy=+5 (MS Y up) → screen (110, 45) */
    cmp cx, 110
    jne .fail_pos
    cmp dx, 45
    jne .fail_pos
    cmp bx, 1
    jne .fail_btn

    /* release button */
    cli
    mov dx, INJECT
    xor al, al
    out dx, al
    xor al, al
    out dx, al
    xor al, al
    out dx, al
    mov ax, 0x0003
    int 0x33
    sti
    test bx, bx
    jnz .fail_btn

    mov ah, 0x09
    lea dx, [msg_ok]
    int 0x21
    mov ax, 0x4C00
    int 0x21

.fail_nodrv:
    mov ah, 0x09
    lea dx, [msg_nodrv]
    int 0x21
    mov ax, 0x4C01
    int 0x21

.fail_pos:
    mov ah, 0x09
    lea dx, [msg_pos]
    int 0x21
    mov ax, 0x4C01
    int 0x21

.fail_btn:
    mov ah, 0x09
    lea dx, [msg_btn]
    int 0x21
    mov ax, 0x4C01
    int 0x21

msg_ok:    .ascii "MOUSE OK\r\n$"
msg_nodrv: .ascii "MOUSE FAIL: no driver\r\n$"
msg_pos:   .ascii "MOUSE FAIL: position\r\n$"
msg_btn:   .ascii "MOUSE FAIL: buttons\r\n$"
