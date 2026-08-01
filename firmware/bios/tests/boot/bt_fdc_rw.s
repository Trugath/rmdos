.code16
.intel_syntax noprefix
.section .text
.global _start

/* Write sector 2 track 0, read back, compare. Requires guest FDC INT 13. */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ah, 0x00
    mov dl, 0x00
    int 0x13
    jc .fail_reset

    /* Fill 0000:9000 with pattern */
    mov di, 0x9000
    mov cx, 512
    mov al, 0xA5
    rep stosb

    mov ax, 0x0301
    mov bx, 0x9000
    mov cx, 0x0002              /* cyl 0 sector 2 */
    mov dx, 0x0000
    int 0x13
    jc .fail_write

    /* Clear read buffer */
    mov di, 0x9200
    mov cx, 512
    xor al, al
    rep stosb

    mov ax, 0x0201
    mov bx, 0x9200
    mov cx, 0x0002
    mov dx, 0x0000
    int 0x13
    jc .fail_read

    mov si, 0x9000
    mov di, 0x9200
    mov cx, 512
    repe cmpsb
    jne .fail_cmp

    /* Verify AH=04 */
    mov ax, 0x0401
    mov bx, 0x9000
    mov cx, 0x0002
    mov dx, 0x0000
    int 0x13
    jc .fail_ver

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_reset:
    push cs
    pop ds
    mov si, offset msg_rst
    call fail_and_halt
.fail_write:
    push cs
    pop ds
    mov si, offset msg_wr
    call fail_and_halt
.fail_read:
    push cs
    pop ds
    mov si, offset msg_rd
    call fail_and_halt
.fail_cmp:
    push cs
    pop ds
    mov si, offset msg_cmp
    call fail_and_halt
.fail_ver:
    push cs
    pop ds
    mov si, offset msg_ver
    call fail_and_halt

name:
    .asciz "bt_fdc_rw"
msg_rst:
    .asciz "bt_fdc_rw:reset"
msg_wr:
    .asciz "bt_fdc_rw:write"
msg_rd:
    .asciz "bt_fdc_rw:read"
msg_cmp:
    .asciz "bt_fdc_rw:cmp"
msg_ver:
    .asciz "bt_fdc_rw:verify"

.include "firmware/bios/tests/boot/common.inc"
