.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * AH=05 format track 1 head 0 (not track 0 — formatting cyl 0 destroys the
 * boot sector when the emulator persists image writes), then AH=02 readback.
 */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ah, 0x00
    xor dl, dl
    int 0x13
    jc .fail_reset

    /* ID table at 0x9000: 9 × (C=1,H=0,R,N=2) */
    mov di, 0x9000
    mov bl, 1
.idt:
    mov al, 1
    stosb                       /* C */
    xor al, al
    stosb                       /* H */
    mov al, bl
    stosb                       /* R */
    mov al, 2
    stosb                       /* N */
    inc bl
    cmp bl, 10
    jb .idt

    mov ah, 0x05
    mov al, 9
    mov bx, 0x9000
    mov cx, 0x0100              /* cyl 1 */
    xor dx, dx                  /* head 0 drive 0 */
    int 0x13
    jc .fail_fmt

    mov ax, 0x0201
    mov bx, 0x9200
    mov cx, 0x0101              /* cyl 1 sector 1 */
    xor dx, dx
    int 0x13
    jc .fail_read

    /* After format fill should be 0xF6 */
    cmp byte ptr [0x9200], 0xF6
    jne .fail_fill

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_reset:
    push cs
    pop ds
    mov si, offset msg_rst
    call fail_and_halt
.fail_fmt:
    push cs
    pop ds
    mov si, offset msg_fmt
    call fail_and_halt
.fail_read:
    push cs
    pop ds
    mov si, offset msg_rd
    call fail_and_halt
.fail_fill:
    push cs
    pop ds
    mov si, offset msg_fill
    call fail_and_halt

name:
    .asciz "bt_fdc_fmt"
msg_rst:
    .asciz "bt_fdc_fmt:reset"
msg_fmt:
    .asciz "bt_fdc_fmt:fmt"
msg_rd:
    .asciz "bt_fdc_fmt:read"
msg_fill:
    .asciz "bt_fdc_fmt:fill"

.include "firmware/bios/tests/boot/common.inc"
