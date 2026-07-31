.code16
.intel_syntax noprefix
.section .text
.global _start

/* DEL.COM — delete file named in PSP tail. */

_start:
    push cs
    pop ds

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
    lea di, [name]
.copy:
    lodsb
    cmp al, ' '
    je .end
    cmp al, 0x0D
    je .end
    cmp al, 0
    je .end
    stosb
    jmp .copy
.end:
    mov byte ptr [di], 0

    mov ah, 0x41
    lea dx, [name]
    int 0x21
    jc .err
    mov ah, 0x09
    lea dx, [msg_ok]
    int 0x21
    mov ax, 0x4C00
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
    .ascii "deleted\r\n$"
msg_err:
    .ascii "DEL failed\r\n$"
msg_u:
    .ascii "DEL file\r\n$"
name:
    .space 16, 0
