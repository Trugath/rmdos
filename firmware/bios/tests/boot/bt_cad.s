.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * CAD warm-boot trampoline at F000:EA82 → far JMP F000:E05B.
 * Also verify cad_main warm-flag path is reachable via BDA write probe
 * (set magic, clear, ensure trampoline target is POST entry).
 */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0xF000
    mov es, ax

    /* EA82: JMP FAR F000:E05B → EA 5B E0 00 F0 */
    cmp byte ptr es:[0xEA82], 0xEA
    jne .fail_op
    cmp word ptr es:[0xEA83], 0xE05B
    jne .fail_tgt
    cmp word ptr es:[0xEA85], 0xF000
    jne .fail_tgt

    /* POST entry trampoline at E05B exists (near JMP or code). */
    mov al, es:[0xE05B]
    cmp al, 0xE9
    je .ok_post
    cmp al, 0xEB
    je .ok_post
    cmp al, 0xEA
    je .ok_post
    /* Accept any non-zero / non-FF opcode as present code */
    cmp al, 0x00
    je .fail_post
    cmp al, 0xFF
    je .fail_post
.ok_post:

    /* Warm-boot magic location is writable in BDA */
    mov ax, 0x0040
    mov ds, ax
    mov word ptr [0x0072], 0x1234
    cmp word ptr [0x0072], 0x1234
    jne .fail_bda
    mov word ptr [0x0072], 0

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_op:
    push cs
    pop ds
    mov si, offset msg_op
    call fail_and_halt
.fail_tgt:
    push cs
    pop ds
    mov si, offset msg_tgt
    call fail_and_halt
.fail_post:
    push cs
    pop ds
    mov si, offset msg_post
    call fail_and_halt
.fail_bda:
    push cs
    pop ds
    mov si, offset msg_bda
    call fail_and_halt

name:
    .asciz "bt_cad"
msg_op:
    .asciz "bt_cad:op"
msg_tgt:
    .asciz "bt_cad:tgt"
msg_post:
    .asciz "bt_cad:post"
msg_bda:
    .asciz "bt_cad:bda"

.include "firmware/bios/tests/boot/common.inc"
