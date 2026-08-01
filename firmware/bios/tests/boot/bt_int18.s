.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * INT 18h teletypes "rmDOS: no ROM BASIC\r\n" then HLT.
 * Hook INT 10h AH=0E: count matching prefix chars; PASS when complete.
 */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    mov byte ptr [0x7e10], 0
    mov word ptr [0x10 * 4], offset int10_hook
    mov word ptr [0x10 * 4 + 2], cs
    sti
    int 0x18
    push cs
    pop ds
    mov si, offset msg_ret
    call fail_and_halt

int10_hook:
    cmp ah, 0x0E
    jne .done
    push ax
    push bx
    push ds
    push cs
    pop ds
    xor bx, bx
    mov bl, [match_idx]
    mov ah, al
    mov al, [expect + bx]
    test al, al
    jz .pop_ok
    cmp al, ah
    jne .reset
    inc byte ptr [match_idx]
    cmp byte ptr [expect + bx + 1], 0
    jne .pop_ok
    pop ds
    pop bx
    pop ax
    push cs
    pop ds
    mov si, offset name
    call pass_and_halt
.reset:
    mov byte ptr [match_idx], 0
.pop_ok:
    pop ds
    pop bx
    pop ax
.done:
    iret

match_idx:
    .byte 0
expect:
    .asciz "rmDOS: no ROM"
name:
    .asciz "bt_int18"
msg_ret:
    .asciz "bt_int18:ret"

.include "firmware/bios/tests/boot/common.inc"
