.code16
.intel_syntax noprefix
.section .text
.global _start

/* C800: AH=00 reset; AH=03 write cyl1; AH=02 readback; AH=01 status clear */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ah, 0x00
    mov dl, 0x80
    int 0x13
    jc .fail_reset

    /* Pattern buffer at 9000 */
    mov di, 0x9000
    mov cx, 256
    mov ax, 0xA55A
.fill:
    stosw
    loop .fill

    mov ax, 0x0301
    mov bx, 0x9000
    mov cx, 0x0101              /* cyl 1 sec 1 */
    xor dx, dx
    mov dl, 0x80
    int 0x13
    jc .fail_write

    /* Clear dest */
    mov di, 0x9200
    mov cx, 256
    xor ax, ax
    rep stosw

    mov ax, 0x0201
    mov bx, 0x9200
    mov cx, 0x0101
    xor dx, dx
    mov dl, 0x80
    int 0x13
    jc .fail_read

    mov si, 0x9000
    mov di, 0x9200
    mov cx, 256
.cmp:
    cmpsw
    jne .fail_data
    loop .cmp

    mov ah, 0x01
    mov dl, 0x80
    int 0x13
    jc .fail_status
    test ah, ah
    jnz .fail_status

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_reset:
    push cs
    pop ds
    mov si, offset msg_reset
    call fail_and_halt
.fail_write:
    push cs
    pop ds
    mov si, offset msg_write
    call fail_and_halt
.fail_read:
    push cs
    pop ds
    mov si, offset msg_read
    call fail_and_halt
.fail_data:
    push cs
    pop ds
    mov si, offset msg_data
    call fail_and_halt
.fail_status:
    push cs
    pop ds
    mov si, offset msg_status
    call fail_and_halt

name:
    .asciz "bt_hd_rw"
msg_reset:
    .asciz "bt_hd_rw:reset"
msg_write:
    .asciz "bt_hd_rw:write"
msg_read:
    .asciz "bt_hd_rw:read"
msg_data:
    .asciz "bt_hd_rw:data"
msg_status:
    .asciz "bt_hd_rw:status"

.include "firmware/bios/tests/boot/common.inc"
