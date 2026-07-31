.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 10h set-mode for graphics modes 4,5,6 */

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

    mov bp, 4
.loop:
    cmp bp, 7
    jae .ok
    mov ax, bp
    int 0x10
    mov ah, 0x0F
    int 0x10
    mov bx, bp
    cmp al, bl
    jne .fail
    mov cx, BDA_SEG
    mov es, cx
    cmp byte ptr es:[BDA_CRT_MODE], bl
    jne .fail
    mov al, es:[BDA_CRT_MODE_REG]
    test al, 0x02
    jz .fail
    cmp bl, 6
    jne .next
    test al, 0x10
    jz .fail
.next:
    inc bp
    jmp .loop
.ok:
    /* restore text */
    mov ax, 0x0003
    int 0x10
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
    .asciz "bt_modes_gfx"
msg:
    .asciz "bt_modes_gfx:fail"

.include "firmware/bios/tests/boot/common.inc"
