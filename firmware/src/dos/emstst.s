.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * EMSTST.COM — EMS smoke: install check, alloc/map/write/read/dealloc.
 */

_start:
    /* INT 67 vector → device name EMMXXXX0 at ES:000A */
    mov ax, 0x3567
    int 0x21
    cmp word ptr es:[0x0A], 0x4D45       /* 'EM' */
    jne .fail
    cmp word ptr es:[0x0C], 0x584D       /* 'MX' */
    jne .fail
    cmp word ptr es:[0x0E], 0x5858       /* 'XX' */
    jne .fail
    cmp word ptr es:[0x10], 0x3058       /* 'X0' */
    jne .fail

    mov ah, 0x40
    int 0x67
    test ah, ah
    jnz .fail

    mov ah, 0x46
    int 0x67
    test ah, ah
    jnz .fail
    cmp al, 0x32
    jne .fail

    mov ah, 0x41
    int 0x67
    test ah, ah
    jnz .fail
    cmp bx, 0xD000
    jne .fail
    mov word ptr [frame], bx

    mov ah, 0x42
    int 0x67
    test ah, ah
    jnz .fail
    cmp bx, 2
    jb .fail

    mov bx, 2
    mov ah, 0x43
    int 0x67
    test ah, ah
    jnz .fail
    mov word ptr [handle], dx

    /* map logical 0 → phys 0 */
    mov dx, word ptr [handle]
    xor bx, bx
    xor al, al
    mov ah, 0x44
    int 0x67
    test ah, ah
    jnz .fail_free

    /* map logical 1 → phys 1 */
    mov dx, word ptr [handle]
    mov bx, 1
    mov al, 1
    mov ah, 0x44
    int 0x67
    test ah, ah
    jnz .fail_free

    mov es, word ptr [frame]
    mov byte ptr es:[0], 0xA5
    mov byte ptr es:[0x4000], 0x5A
    cmp byte ptr es:[0], 0xA5
    jne .fail_free
    cmp byte ptr es:[0x4000], 0x5A
    jne .fail_free

    /* remap phys0 to logical 1 — should see 5A */
    mov dx, word ptr [handle]
    mov bx, 1
    xor al, al
    mov ah, 0x44
    int 0x67
    test ah, ah
    jnz .fail_free
    cmp byte ptr es:[0], 0x5A
    jne .fail_free

    mov dx, word ptr [handle]
    mov ah, 0x45
    int 0x67
    test ah, ah
    jnz .fail

    mov ah, 0x09
    lea dx, [msg_ok]
    int 0x21
    mov ax, 0x4C00
    int 0x21

.fail_free:
    mov dx, word ptr [handle]
    mov ah, 0x45
    int 0x67
.fail:
    mov ah, 0x09
    lea dx, [msg_fail]
    int 0x21
    mov ax, 0x4C01
    int 0x21

handle:
    .word 0
frame:
    .word 0
msg_ok:
    .ascii "EMS OK\r\n$"
msg_fail:
    .ascii "EMS FAIL\r\n$"
