.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * DIR.COM — list root directory via INT 21h FindFirst/Next.
 */

_start:
    mov ah, 0x1A
    lea dx, [dta]
    int 0x21

    mov ah, 0x4E
    lea dx, [pattern]
    xor cx, cx
    int 0x21
    jc .done

.loop:
    lea si, [dta + 0x1E]
.print:
    lodsb
    test al, al
    jz .nl
    mov dl, al
    mov ah, 0x02
    int 0x21
    jmp .print
.nl:
    mov dl, 0x0D
    mov ah, 0x02
    int 0x21
    mov dl, 0x0A
    mov ah, 0x02
    int 0x21

    mov ah, 0x4F
    int 0x21
    jnc .loop

.done:
    mov ax, 0x4C00
    int 0x21

pattern:
    .asciz "*.*"
dta:
    .space 128, 0
