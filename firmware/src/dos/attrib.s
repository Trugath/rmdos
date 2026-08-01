.code16
.intel_syntax noprefix
.section .text
.global _start
/* ATTRIB [+R|-R] [+A|-A] [+S|-S] [+H|-H] file */
_start:
    push cs
    pop ds
    push cs
    pop es
    mov byte ptr [mask], 0
    mov byte ptr [bits], 0
    mov byte ptr [set_mode], 0
    mov si, 0x81
.next:
    call skip
    cmp byte ptr [si], 0
    je .go
    cmp byte ptr [si], 13
    je .go
    mov al, [si]
    cmp al, '+'
    je .opt
    cmp al, '-'
    je .opt
    lea di, [pattern]
.name:
    lodsb
    cmp al, ' '
    je .next
    cmp al, 13
    je .go
    test al, al
    jz .go
    stosb
    jmp .name
.opt:
    mov bl, al
    inc si
    lodsb
    and al, 0xDF
    cmp al, 'R'
    je .r
    cmp al, 'A'
    je .a
    cmp al, 'S'
    je .s
    cmp al, 'H'
    jne .next
    mov cl, 2
    jmp .set
.r: mov cl, 1
    jmp .set
.a: mov cl, 0x20
    jmp .set
.s: mov cl, 4
.set:
    or byte ptr [mask], cl
    cmp bl, '+'
    jne .next
    or byte ptr [bits], cl
    mov byte ptr [set_mode], 1
    jmp .next
.go:
    cmp byte ptr [pattern], 0
    je usage
    mov ah, 0x1A
    lea dx, [dta]
    int 0x21
    mov ah, 0x4E
    lea dx, [pattern]
    mov cx, 0x37
    int 0x21
    jc fail
.entry:
    cmp byte ptr [set_mode], 0
    je .show
    mov al, [dta+0x15]
    and al, 0x3F
    mov ah, [mask]
    not ah
    and al, ah
    or al, [bits]
    mov cx, ax
    mov ah, 0x43
    mov al, 1
    lea dx, [dta+0x1E]
    int 0x21
    jc fail
.show:
    mov dl, [dta+0x15]
    test dl, 1
    jz .nr
    mov al, 'R'
    jmp .pr
.nr: mov al, '-'
.pr: mov ah, 0x02
    mov dl, al
    int 0x21
    mov dl, [dta+0x15]
    test dl, 0x20
    jz .na
    mov al, 'A'
    jmp .pa
.na: mov al, '-'
.pa: mov ah, 0x02
    mov dl, al
    int 0x21
    mov dl, [dta+0x15]
    test dl, 4
    jz .ns
    mov al, 'S'
    jmp .ps
.ns: mov al, '-'
.ps: mov ah, 0x02
    mov dl, al
    int 0x21
    mov dl, [dta+0x15]
    test dl, 2
    jz .nh
    mov al, 'H'
    jmp .ph
.nh: mov al, '-'
.ph: mov ah, 0x02
    mov dl, al
    int 0x21
    mov ah, 0x09
    lea dx, [sep]
    int 0x21
    lea si, [dta+0x1E]
    mov cx, 13
.pn:
    lodsb
    test al, al
    jz .pnd
    mov dl, al
    mov ah, 0x02
    int 0x21
    loop .pn
.pnd:
    mov ah, 0x09
    lea dx, [crlf]
    int 0x21
    mov ah, 0x4F
    int 0x21
    jnc .entry
    mov ax, 0x4C00
    int 0x21
skip:
    cmp byte ptr [si], ' '
    jne .ret
    inc si
    jmp skip
.ret: ret
usage:
    lea dx, [msg_u]
    jmp print_exit
fail:
    lea dx, [msg_e]
print_exit:
    mov ah, 9
    int 0x21
    mov ax, 0x4C01
    int 0x21
mask: .byte 0
bits: .byte 0
set_mode: .byte 0
pattern: .space 64, 0
dta: .space 128, 0
sep: .ascii " $"
crlf: .ascii "\r\n$"
msg_u: .ascii "ATTRIB [+R|-R] [+A|-A] [+S|-S] [+H|-H] file\r\n$"
msg_e: .ascii "ATTRIB failed\r\n$"
