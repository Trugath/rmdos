.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 10h set-mode for text modes 0..3 */

.equ BDA_SEG, 0x0040
.equ BDA_CRT_MODE, 0x49
.equ BDA_CRT_MODE_REG, 0x65

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    xor bp, bp
.loop:
    cmp bp, 4
    jae .ok
    mov ax, bp
    int 0x10                    /* AH=0 already */
    mov ah, 0x0F
    int 0x10
    mov bx, bp
    cmp al, bl
    jne .fail
    mov cx, BDA_SEG
    mov es, cx
    cmp byte ptr es:[BDA_CRT_MODE], bl
    jne .fail
    test byte ptr es:[BDA_CRT_MODE_REG], 0x02
    jnz .fail                   /* must be text */
    /* 80-col modes set bit0 */
    cmp bl, 2
    jb .next
    test byte ptr es:[BDA_CRT_MODE_REG], 0x01
    jz .fail
.next:
    inc bp
    jmp .loop
.ok:
    push cs
    pop ds
    mov si, offset name
    call pass_and_halt
.fail:
    push cs
    pop ds
    mov si, offset msg
    call fail_and_halt

name:
    .asciz "bt_modes_text"
msg:
    .asciz "bt_modes_text:fail"

.include "firmware/bios/tests/boot/common.inc"
