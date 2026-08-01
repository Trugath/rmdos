.code16
.intel_syntax noprefix
.section .text
.global _start
/* MOVE src dst.  DOS rename handles the cheap, same-volume case first. */
_start:
    push cs
    pop ds
    push cs
    pop es
    mov si, 0x81
    lea di, [src]
    call token
    jc usage
    lea di, [dst]
    call token
    jc usage
    mov ah, 0x56
    lea dx, [src]
    lea di, [dst]
    int 0x21
    jnc ok
    mov ax, 0x3D00
    lea dx, [src]
    int 0x21
    jc fail
    mov [hin], bx
    mov ah, 0x3C
    xor cx, cx
    lea dx, [dst]
    int 0x21
    jc close_in
    mov [hout], bx
.copy:
    mov ah, 0x3F
    mov bx, [hin]
    mov cx, 128
    lea dx, [buf]
    int 0x21
    jc close_both
    test ax, ax
    jz .copied
    mov cx, ax
    mov ah, 0x40
    mov bx, [hout]
    lea dx, [buf]
    int 0x21
    jc close_both
    jmp .copy
.copied:
    mov ah, 0x3E
    mov bx, [hout]
    int 0x21
    mov ah, 0x3E
    mov bx, [hin]
    int 0x21
    mov ah, 0x41
    lea dx, [src]
    int 0x21
    jc fail
ok:
    lea dx, [msg_ok]
    mov ah, 9
    int 0x21
    mov ax, 0x4C00
    int 0x21
close_both:
    mov ah, 0x3E
    mov bx, [hout]
    int 0x21
close_in:
    mov ah, 0x3E
    mov bx, [hin]
    int 0x21
fail:
    lea dx, [msg_e]
    mov ah, 9
    int 0x21
    mov ax, 0x4C01
    int 0x21
usage:
    lea dx, [msg_u]
    mov ah, 9
    int 0x21
    mov ax, 0x4C01
    int 0x21
/* SI -> tail, DI output. CF means no token. */
token:
.tok_skip:
    lodsb
    cmp al, ' '
    je .tok_skip
    cmp al, 13
    je .bad
    test al, al
    jz .bad
    dec si
.tok_copy:
    lodsb
    cmp al, ' '
    je .done
    cmp al, 13
    je .done
    test al, al
    jz .done
    stosb
    jmp .tok_copy
.done:
    mov byte ptr [di], 0
    clc
    ret
.bad: stc
    ret
src: .space 64, 0
dst: .space 64, 0
hin: .word 0
hout: .word 0
buf: .space 128, 0
msg_ok: .ascii "moved\r\n$"
msg_e: .ascii "MOVE failed\r\n$"
msg_u: .ascii "MOVE src dst\r\n$"
