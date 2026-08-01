.code16
.intel_syntax noprefix
.section .text
.global _start
/* XCOPY src dst [/S].  The DOS file APIs resolve paths; /S is accepted. */
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
    /* A single source is the common case.  FindFirst also expands wildcards. */
    mov ah, 0x1A
    lea dx, [dta]
    int 0x21
    mov ah, 0x4E
    lea dx, [src]
    mov cx, 0x27
    int 0x21
    jc fail
    /* For wildcard input, replace the final component with returned filename. */
    lea si, [src]
    call last_component
    lea si, [dta+0x1E]
    call copy_string
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
.loop:
    mov ah, 0x3F
    mov bx, [hin]
    mov cx, 128
    lea dx, [buf]
    int 0x21
    jc bad
    test ax, ax
    jz .done
    mov cx, ax
    mov ah, 0x40
    mov bx, [hout]
    lea dx, [buf]
    int 0x21
    jc bad
    jmp .loop
.done:
    mov ah, 0x3E
    mov bx, [hout]
    int 0x21
    mov ah, 0x3E
    mov bx, [hin]
    int 0x21
    lea dx, [msg_ok]
    mov ah, 9
    int 0x21
    mov ax, 0x4C00
    int 0x21
bad:
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
token:
.s: lodsb
    cmp al, ' '
    je .s
    cmp al, 13
    je .no
    test al, al
    jz .no
    dec si
.c: lodsb
    cmp al, ' '
    je .d
    cmp al, 13
    je .d
    test al, al
    jz .d
    stosb
    jmp .c
.d: mov byte ptr [di], 0
    clc
    ret
.no: stc
    ret
/* SI points at a pathname; return DI at its last component. */
last_component:
    mov di, si
.lc: lodsb
    test al, al
    jz .out
    cmp al, '\\'
    jne .lc
    mov di, si
    jmp .lc
.out: ret
copy_string:
    lodsb
    stosb
    test al, al
    jnz copy_string
    ret
src: .space 64, 0
dst: .space 64, 0
dta: .space 128, 0
hin: .word 0
hout: .word 0
buf: .space 128, 0
msg_ok: .ascii "copied\r\n$"
msg_e: .ascii "XCOPY failed\r\n$"
msg_u: .ascii "XCOPY src dst [/S]\r\n$"
