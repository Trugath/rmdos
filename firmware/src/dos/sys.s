.code16
.intel_syntax noprefix
.section .text
.global _start
/*
 * SYS [d:]
 *
 * rmDOS keeps its boot loader and RFAT1 metadata in reserved sectors.  Copy
 * those sectors without touching either FAT.  It preserves existing FAT and
 * root directory contents on the selected volume.
 */
_start:
    push cs
    pop ds
    push cs
    pop es
    mov byte ptr [drive], 0
    mov si, 0x81
    cmp byte ptr [si], ' '
    jne .transfer
    inc si
    mov al, [si]
    and al, 0xDF
    cmp byte ptr [si+1], ':'
    jne usage
    sub al, 'A'
    mov [drive], al
.transfer:
    /* Read/write the boot sector through the absolute disk API. */
    mov al, [drive]
    mov cx, 1
    xor dx, dx
    lea bx, [boot]
    int 0x25
    pop dx
    jc fail
    mov al, [drive]
    mov cx, 1
    xor dx, dx
    lea bx, [boot]
    int 0x26
    pop dx
    jc fail
    /* Reserved sector 1 contains RFAT1 when present. */
    mov al, [drive]
    mov cx, 1
    mov dx, 1
    lea bx, [rfat]
    int 0x25
    pop dx
    jc done
    cmp dword ptr [rfat], 0x54414652 /* RFAT */
    jne done
    mov al, [drive]
    mov cx, 1
    mov dx, 1
    lea bx, [rfat]
    int 0x26
    pop dx
    jc fail
done:
    lea dx, [msg_ok]
    mov ah, 9
    int 0x21
    mov ax, 0x4C00
    int 0x21
usage:
    lea dx, [msg_u]
    mov ah, 9
    int 0x21
    mov ax, 0x4C01
    int 0x21
fail:
    lea dx, [msg_e]
    mov ah, 9
    int 0x21
    mov ax, 0x4C01
    int 0x21
drive: .byte 0
boot: .space 512,0
rfat: .space 512,0
msg_ok: .ascii "System transferred\r\nSYS OK\r\n$"
msg_u: .ascii "SYS [d:]\r\n$"
msg_e: .ascii "SYS failed\r\n$"
