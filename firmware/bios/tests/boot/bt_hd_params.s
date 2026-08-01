.code16
.intel_syntax noprefix
.section .text
.global _start

/* C800 Fixed Disk ROM: INT 13h AH=08 DL=80 geometry for XT ~10MB image */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ah, 0x08
    mov dl, 0x80
    int 0x13
    jc .fail_params

    /* SPT in CL[5:0] — XT default 17 */
    mov al, cl
    and al, 0x3F
    cmp al, 17
    jne .fail_params

    /* Max head in DH — 4 heads → 3 */
    cmp dh, 3
    jne .fail_params

    /* At least one HD */
    cmp dl, 1
    jb .fail_params

    /* Max cylinder low byte: 306-1 = 305 = 0x131 → CH=0x31 */
    cmp ch, 0x31
    jne .fail_params

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_params:
    push cs
    pop ds
    mov si, offset msg_params
    call fail_and_halt

name:
    .asciz "bt_hd_params"
msg_params:
    .asciz "bt_hd_params:params"

.include "firmware/bios/tests/boot/common.inc"
