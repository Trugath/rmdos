.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * INT 16h AH=05 stuff + AH=10/11/12 mapped to 00/01/02.
 */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    /* Stuff 'A' (scan 1Eh, ascii 41h). */
    mov cx, 0x1E41
    mov ah, 0x05
    int 0x16
    jc .fail_stuff

    /* AH=11h status → ZF clear, AL='A'. */
    mov ah, 0x11
    int 0x16
    jz .fail_status
    cmp al, 'A'
    jne .fail_status
    cmp ah, 0x1E
    jne .fail_status

    /* AH=10h read → consume. */
    mov ah, 0x10
    int 0x16
    cmp ax, 0x1E41
    jne .fail_read

    /* Buffer empty via AH=11h. */
    mov ah, 0x11
    int 0x16
    jnz .fail_empty

    /* AH=12h shift flags mirrors AH=02. */
    mov ax, 0x0040
    mov es, ax
    mov byte ptr es:[0x17], 0x20
    mov ah, 0x12
    int 0x16
    cmp al, 0x20
    jne .fail_shift
    mov byte ptr es:[0x17], 0

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_stuff:
    push cs
    pop ds
    mov si, offset msg_stuff
    call fail_and_halt
.fail_status:
    push cs
    pop ds
    mov si, offset msg_status
    call fail_and_halt
.fail_read:
    push cs
    pop ds
    mov si, offset msg_read
    call fail_and_halt
.fail_empty:
    push cs
    pop ds
    mov si, offset msg_empty
    call fail_and_halt
.fail_shift:
    push cs
    pop ds
    mov si, offset msg_shift
    call fail_and_halt

name:
    .asciz "bt_kbd_ext"
msg_stuff:
    .asciz "bt_kbd_ext:stuff"
msg_status:
    .asciz "bt_kbd_ext:status"
msg_read:
    .asciz "bt_kbd_ext:read"
msg_empty:
    .asciz "bt_kbd_ext:empty"
msg_shift:
    .asciz "bt_kbd_ext:shift"

.include "firmware/bios/tests/boot/common.inc"
