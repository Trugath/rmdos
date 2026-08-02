.code16
.intel_syntax noprefix
.section .text
.global _start

/* AH=15 DASD type + AH=16 change-line + AH=17/18 media smoke */

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

    /* AH=17 set DASD type */
    mov ah, 0x17
    mov al, 3
    xor dl, dl
    int 0x13
    jc .fail_17

    /* AH=18 set media (1.44M: 80 cyl / 18 spt) */
    mov ah, 0x18
    mov ch, 79
    mov cl, 18
    xor dl, dl
    int 0x13
    jc .fail_18
    mov ax, es
    test ax, ax
    jz .fail_18

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_dasd:
    push cs
    pop ds
    mov si, offset msg_dasd
    call fail_and_halt
.fail_17:
    push cs
    pop ds
    mov si, offset msg_17
    call fail_and_halt
.fail_18:
    push cs
    pop ds
    mov si, offset msg_18
    call fail_and_halt

name:
    .asciz "bt_fdc_type"
msg_dasd:
    .asciz "bt_fdc_type:dasd"
msg_17:
    .asciz "bt_fdc_type:ah17"
msg_18:
    .asciz "bt_fdc_type:ah18"

.include "firmware/bios/tests/boot/common.inc"
