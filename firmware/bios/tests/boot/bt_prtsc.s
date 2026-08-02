.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * INT 5 Print Screen: with working INT 17 LPT1, status at 0000:0500 becomes 00h.
 */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov byte ptr [0x0500], 0
    int 0x05
    cmp byte ptr [0x0500], 0
    jne .fail_status

    /* Re-entry while "busy" is ignored — force status=1 then INT 5 keeps it. */
    mov byte ptr [0x0500], 1
    int 0x05
    cmp byte ptr [0x0500], 1
    jne .fail_busy

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_status:
    push cs
    pop ds
    mov si, offset msg_status
    call fail_and_halt
.fail_busy:
    push cs
    pop ds
    mov si, offset msg_busy
    call fail_and_halt

name:
    .asciz "bt_prtsc"
msg_status:
    .asciz "bt_prtsc:status"
msg_busy:
    .asciz "bt_prtsc:busy"

.include "firmware/bios/tests/boot/common.inc"
