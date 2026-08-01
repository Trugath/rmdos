.code16
.intel_syntax noprefix
.section .text
.global _start

/* AH=15 DASD type + AH=16 change-line smoke */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ah, 0x15
    xor dl, dl
    int 0x13
    jc .fail_dasd
    cmp ah, 0x02
    jne .fail_dasd

    mov ah, 0x16
    xor dl, dl
    int 0x13
    /* CF clear and AH=0 if no change; CF set AH=06 if change — either ok */
    /* Just ensure call returns without hang */

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_dasd:
    push cs
    pop ds
    mov si, offset msg_dasd
    call fail_and_halt

name:
    .asciz "bt_fdc_type"
msg_dasd:
    .asciz "bt_fdc_type:dasd"

.include "firmware/bios/tests/boot/common.inc"
