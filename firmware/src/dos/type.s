.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * TYPE.COM — print a text file. Filename from PSP command tail (81h).
 */

_start:
    push cs
    pop ds
    push cs
    pop es

    /* skip leading spaces in tail */
    mov si, 0x81
.skip:
    lodsb
    cmp al, ' '
    je .skip
    cmp al, 0x0D
    je .usage
    cmp al, 0
    je .usage
    dec si

    /* copy name to pathbuf until space/CR */
    lea di, [pathbuf]
.copy:
    lodsb
    cmp al, ' '
    je .endname
    cmp al, 0x0D
    je .endname
    cmp al, 0
    je .endname
    stosb
    jmp .copy
.endname:
    mov byte ptr [di], 0

    mov ah, 0x3D
    xor al, al
    lea dx, [pathbuf]
    int 0x21
    jc .err
    mov [handle], bx

.read:
    mov ah, 0x3F
    mov bx, [handle]
    mov cx, 1
    lea dx, [one]
    int 0x21
    jc .close
    test ax, ax
    jz .close
    mov dl, [one]
    mov ah, 0x02
    int 0x21
    jmp .read

.close:
    mov ah, 0x3E
    mov bx, [handle]
    int 0x21
    mov ax, 0x4C00
    int 0x21

.usage:
    mov ah, 0x09
    lea dx, [msg_usage]
    int 0x21
    mov ax, 0x4C01
    int 0x21

.err:
    mov ah, 0x09
    lea dx, [msg_err]
    int 0x21
    mov ax, 0x4C01
    int 0x21

msg_usage:
    .ascii "TYPE file\r\n$"
msg_err:
    .ascii "TYPE: open failed\r\n$"
pathbuf:
    .space 16, 0
handle:
    .word 0
one:
    .byte 0
