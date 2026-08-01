.code16
.intel_syntax noprefix
.section .text
.global _start
/* LABEL [d:][label].  A FAT volume label is a root entry with attribute 08h. */
_start:
    push cs
    pop ds
    push cs
    pop es
    mov si, 0x81
    call skip
    cmp byte ptr [si], 13
    je show
    cmp byte ptr [si], 0
    je show
    lea di, [label]
.copy:
    lodsb
    cmp al, ' '
    je set
    cmp al, 13
    je set
    test al, al
    jz set
    stosb
    jmp .copy
set:
    mov byte ptr [di], 0
    /* Remove an old label if DOS exposes one under its printable name. */
    mov ah, 0x1A
    lea dx, [dta]
    int 0x21
    mov ah, 0x4E
    lea dx, [all]
    mov cx, 8
    int 0x21
    jc .create
    lea dx, [dta+0x1E]
    mov ah, 0x41
    int 0x21
.create:
    mov ah, 0x3C
    mov cx, 8
    lea dx, [label]
    int 0x21
    jc fail
    mov ah, 0x3E
    int 0x21
    lea dx, [msg_ok]
    mov ah, 9
    int 0x21
    mov ax, 0x4C00
    int 0x21
show:
    mov ah, 0x1A
    lea dx, [dta]
    int 0x21
    mov ah, 0x4E
    lea dx, [all]
    mov cx, 8
    int 0x21
    jc .none
    lea dx, [msg_label]
    mov ah, 9
    int 0x21
    lea dx, [dta+0x1E]
    int 0x21
    lea dx, [crlf]
    int 0x21
    mov ax, 0x4C00
    int 0x21
.none:
    lea dx, [msg_none]
    mov ah, 9
    int 0x21
    mov ax, 0x4C00
    int 0x21
fail:
    lea dx, [msg_err]
    mov ah, 9
    int 0x21
    mov ax, 0x4C01
    int 0x21
skip:
    cmp byte ptr [si], ' '
    jne .r
    inc si
    jmp skip
.r: ret
all: .asciz "*.*"
label: .space 12, 0
dta: .space 128, 0
msg_ok: .ascii "Volume label set\r\n$"
msg_label: .ascii "Volume label is $"
msg_none: .ascii "Volume has no label\r\n$"
msg_err: .ascii "LABEL failed\r\n$"
crlf: .ascii "\r\n$"
