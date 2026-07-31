.code16
.intel_syntax noprefix
.section .text
.global _start

/* Hook INT 1Ch; wait for BIOS timer to invoke it */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00

    mov word ptr [0x0500], 0    /* hook counter */

    /* install INT 1Ch → our hook at CS:hook_1c */
    mov ax, cs
    mov word ptr [0x1C * 4], offset hook_1c
    mov word ptr [0x1C * 4 + 2], ax
    sti

    mov cx, 0
.wait:
    cmp word ptr [0x0500], 0
    jne .got
    loop .wait
    /* CX wrapped; one more long wait via HLT */
    mov bx, 200
.hlt_wait:
    hlt
    cmp word ptr [0x0500], 0
    jne .got
    dec bx
    jnz .hlt_wait
    jmp .fail

.got:
    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail:
    push cs
    pop ds
    mov si, offset msg_fail
    call fail_and_halt

hook_1c:
    push ds
    push ax
    xor ax, ax
    mov ds, ax
    inc word ptr [0x0500]
    pop ax
    pop ds
    iret

name:
    .asciz "bt_int1c"
msg_fail:
    .asciz "bt_int1c:nohook"

.include "firmware/bios/tests/boot/common.inc"
