.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * INT 19h: corrupt floppy AA55 → fall through to HD MBR.
 * HD sector 0 carries a PIC payload that prints PASS and shuts down.
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
    mov dl, 0x80
    int 0x13
    jc .fail_reset

    /* Build HD MBR at 0x9000 from PIC payload */
    push cs
    pop ds
    mov si, offset hd_payload
    mov di, 0x9000
    mov cx, hd_payload_end - hd_payload
    cld
    rep movsb

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov cx, 510 - (hd_payload_end - hd_payload)
    xor al, al
    rep stosb
    mov word ptr [0x9000 + 510], 0xAA55

    mov ax, 0x0301
    mov bx, 0x9000
    xor cx, cx
    inc cx                      /* cyl 0 sec 1 */
    xor dx, dx
    mov dl, 0x80
    int 0x13
    jc .fail_hdwr

    /* Read floppy sector 0, clear signature, write back */
    mov ax, 0x0201
    mov bx, 0x8000
    mov cx, 0x0001
    xor dx, dx
    int 0x13
    jc .fail_fd

    mov word ptr [0x8000 + 510], 0

    mov ax, 0x0301
    mov bx, 0x8000
    mov cx, 0x0001
    xor dx, dx
    int 0x13
    jc .fail_fdwr

    /* Must not return — HD payload emits PASS */
    int 0x19
    jmp .fail_ret

.fail_reset:
    push cs
    pop ds
    mov si, offset msg_rst
    call fail_and_halt
.fail_hdwr:
    push cs
    pop ds
    mov si, offset msg_hd
    call fail_and_halt
.fail_fd:
    push cs
    pop ds
    mov si, offset msg_fd
    call fail_and_halt
.fail_fdwr:
    push cs
    pop ds
    mov si, offset msg_fdw
    call fail_and_halt
.fail_ret:
    push cs
    pop ds
    mov si, offset msg_ret
    call fail_and_halt

msg_rst:
    .asciz "bt_int19_hd:rst"
msg_hd:
    .asciz "bt_int19_hd:hd"
msg_fd:
    .asciz "bt_int19_hd:fd"
msg_fdw:
    .asciz "bt_int19_hd:fdw"
msg_ret:
    .asciz "bt_int19_hd:ret"

/*
 * Position-independent HD boot payload (runs at 0000:7C00).
 * Prints PASS bt_int19_hd and shuts down via port 0x8900.
 */
hd_payload:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    call .hp_puts
    .ascii "PASS bt_int19_hd\r\n"
    .byte 0
.hp_puts:
    pop si
.hp_ploop:
    lodsb
    test al, al
    jz .hp_pdon
    mov ah, al
.hp_wait:
    mov dx, 0x3FD
    in al, dx
    test al, 0x20
    jz .hp_wait
    mov al, ah
    mov dx, 0x3F8
    out dx, al
    jmp .hp_ploop
.hp_pdon:

    call .hp_shut
    .ascii "Shutdown"
    .byte 0
.hp_shut:
    pop si
    mov dx, 0x8900
.hp_sloop:
    lodsb
    test al, al
    jz .hp_hang
    out dx, al
    jmp .hp_sloop
.hp_hang:
    hlt
    jmp .hp_hang
hd_payload_end:

.include "firmware/bios/tests/boot/common.inc"
