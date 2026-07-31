.code16
.intel_syntax noprefix
.section .text
.global _start

/* COPY.COM — COPY src dst (root names, PSP tail). */

_start:
    push cs
    pop ds
    push cs
    pop es

    mov si, 0x81
.skip1:
    lodsb
    cmp al, ' '
    je .skip1
    cmp al, 0x0D
    je .usage
    cmp al, 0
    je .usage
    dec si
    lea di, [src]
.copy1:
    lodsb
    cmp al, ' '
    je .end1
    cmp al, 0x0D
    je .usage
    stosb
    jmp .copy1
.end1:
    mov byte ptr [di], 0

.skip2:
    lodsb
    cmp al, ' '
    je .skip2
    cmp al, 0x0D
    je .usage
    cmp al, 0
    je .usage
    dec si
    lea di, [dst]
.copy2:
    lodsb
    cmp al, ' '
    je .end2
    cmp al, 0x0D
    je .end2
    cmp al, 0
    je .end2
    stosb
    jmp .copy2
.end2:
    mov byte ptr [di], 0

    mov ah, 0x3D
    xor al, al
    lea dx, [src]
    int 0x21
    jc .err
    mov [hin], bx

    mov ah, 0x3C
    xor cx, cx
    lea dx, [dst]
    int 0x21
    jc .err_in
    mov [hout], bx

.loop:
    mov ah, 0x3F
    mov bx, [hin]
    mov cx, 1
    lea dx, [one]
    int 0x21
    jc .done
    test ax, ax
    jz .done
    mov ah, 0x40
    mov bx, [hout]
    mov cx, 1
    lea dx, [one]
    int 0x21
    jc .done
    jmp .loop

.done:
    mov ah, 0x3E
    mov bx, [hout]
    int 0x21
    mov ah, 0x3E
    mov bx, [hin]
    int 0x21
    mov ah, 0x09
    lea dx, [msg_ok]
    int 0x21
    mov ax, 0x4C00
    int 0x21

.err_in:
    mov ah, 0x3E
    mov bx, [hin]
    int 0x21
.err:
    mov ah, 0x09
    lea dx, [msg_err]
    int 0x21
    mov ax, 0x4C01
    int 0x21

.usage:
    mov ah, 0x09
    lea dx, [msg_u]
    int 0x21
    mov ax, 0x4C01
    int 0x21

msg_ok:
    .ascii "copied\r\n$"
msg_err:
    .ascii "COPY failed\r\n$"
msg_u:
    .ascii "COPY src dst\r\n$"
src:
    .space 16, 0
dst:
    .space 16, 0
hin:
    .word 0
hout:
    .word 0
one:
    .byte 0
