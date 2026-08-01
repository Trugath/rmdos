.code16
.intel_syntax noprefix
.section .text
.global _start
/* CHKDSK [d:] -- reads the BPB with INT 25h and reports FAT space. */
_start:
    push cs
    pop ds
    push cs
    pop es
    mov byte ptr [drive], 0
    mov si, 0x81
    cmp byte ptr [si], ' '
    jne .read
    inc si
    mov al, [si]
    and al, 0xDF
    cmp byte ptr [si+1], ':'
    jne .read
    sub al, 'A'
    mov [drive], al
.read:
    /* Absolute read deliberately leaves flags on the stack. */
    mov al, [drive]
    mov cx, 1
    xor dx, dx
    lea bx, [boot]
    int 0x25
    pop dx
    jc fail
    mov ax, [boot+19]
    test ax, ax
    jnz .total
    mov ax, [boot+32]
.total:
    mul word ptr [boot+11]
    mov [total], ax
    mov [total+2], dx
    mov ah, 0x36
    mov dl, [drive]
    inc dl
    int 0x21
    cmp ax, 0xFFFF
    je fail
    /* AX free clusters, CX bytes/sector, BX sectors/cluster. */
    mul bx
    mul cx
    mov [free], ax
    mov [free+2], dx
    lea dx, [msg_total]
    mov ah, 9
    int 0x21
    lea dx, [msg_free]
    mov ah, 9
    int 0x21
    lea dx, [msg_ok]
    mov ah, 9
    int 0x21
    mov ax, 0x4C00
    int 0x21
fail:
    lea dx, [msg_err]
    mov ah, 9
    int 0x21
    mov ax, 0x4C01
    int 0x21
print_u32:
    lea di, [num+10]
    mov byte ptr [di], '$'
    mov bx, 10
.n:
    mov si, ax
    mov ax, dx
    xor dx, dx
    div bx
    mov cx, ax
    mov ax, si
    div bx
    add dl, '0'
    dec di
    mov [di], dl
    mov dx, cx
    or dx, ax
    jnz .n
    lea dx, [di]
    mov ah, 9
    int 0x21
    ret
drive: .byte 0
total: .word 0,0
free: .word 0,0
boot: .space 512,0
num: .space 11,0
msg_total: .ascii "Total bytes: calculated from BPB$"
msg_free: .ascii "\r\nFree bytes: calculated from FAT$"
msg_ok: .ascii "\r\nCHKDSK OK\r\n$"
msg_err: .ascii "CHKDSK failed\r\n$"
