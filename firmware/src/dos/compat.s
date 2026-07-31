.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * COMPAT.COM — exercise DOS 3.3-ish APIs; print COMPAT OK / FAIL.
 */

_start:
    push cs
    pop ds

    /* AH=30 version major=3 */
    mov ah, 0x30
    int 0x21
    cmp al, 3
    jb .fail

    /* AH=25/35 vector roundtrip on unused INT 60h */
    mov ax, 0x3560
    int 0x21
    push es
    push bx
    mov ax, 0x2560
    lea dx, [dummy_isr]
    int 0x21
    mov ax, 0x3560
    int 0x21
    mov ax, es
    mov dx, cs
    cmp ax, dx
    jne .fail_vec
    cmp bx, offset dummy_isr
    jne .fail_vec
    pop dx
    pop ax
    push ds
    mov ds, ax
    mov ax, 0x2560
    int 0x21
    pop ds

    /* AH=19 drive */
    mov ah, 0x19
    int 0x21
    test al, al
    jnz .fail

    /* AH=2A date */
    mov ah, 0x2A
    int 0x21
    cmp cx, 1980
    jb .fail

    /* AH=36 free space */
    mov ah, 0x36
    mov dl, 0
    int 0x21
    cmp ax, 0xFFFF
    je .fail

    /* MKDIR / CHDIR / file in subdir / RMDIR */
    mov ah, 0x39
    lea dx, [dirname]
    int 0x21
    jc .fail

    mov ah, 0x3B
    lea dx, [dirname]
    int 0x21
    jc .fail

    mov ah, 0x3C
    xor cx, cx
    lea dx, [fname]
    int 0x21
    jc .fail
    mov [handle], bx

    mov ah, 0x40
    mov bx, [handle]
    mov cx, 4
    lea dx, [payload]
    int 0x21
    jc .fail

    mov ah, 0x3E
    mov bx, [handle]
    int 0x21

    mov ah, 0x3D
    xor al, al
    lea dx, [fname]
    int 0x21
    jc .fail
    mov [handle], bx

    mov ah, 0x3F
    mov bx, [handle]
    mov cx, 4
    lea dx, [rdbuf]
    int 0x21
    jc .fail
    cmp ax, 4
    jne .fail

    mov ah, 0x3E
    mov bx, [handle]
    int 0x21

    mov ah, 0x3B
    lea dx, [rootpath]
    int 0x21
    jc .fail

    /* FindFirst with subdirectory path pattern */
    mov ah, 0x1A
    lea dx, [dta]
    int 0x21
    mov ah, 0x4E
    xor cx, cx
    lea dx, [findpat]
    int 0x21
    jc .fail

    mov ah, 0x41
    lea dx, [subfile]
    int 0x21
    jc .fail

    mov ah, 0x3A
    lea dx, [dirname]
    int 0x21
    jc .fail

    /* print command tail if any */
    mov si, 0x80
    mov cl, [si]
    test cl, cl
    jz .no_tail
    mov ah, 0x09
    lea dx, [msg_tail]
    int 0x21
.no_tail:

    mov ah, 0x09
    lea dx, [msg_ok]
    int 0x21
    mov ax, 0x4C00
    int 0x21

.fail_vec:
    add sp, 4
.fail:
    mov ah, 0x09
    lea dx, [msg_fail]
    int 0x21
    mov ax, 0x4C01
    int 0x21

dummy_isr:
    iret

dirname:
    .asciz "TMPDIR"
fname:
    .asciz "T.TXT"
subfile:
    .asciz "TMPDIR\\T.TXT"
findpat:
    .asciz "TMPDIR\\*.*"
rootpath:
    .asciz "\\"
payload:
    .ascii "ok!\n"
rdbuf:
    .space 8, 0
dta:
    .space 128, 0
handle:
    .word 0
msg_ok:
    .ascii "COMPAT OK\r\n$"
msg_fail:
    .ascii "COMPAT FAIL\r\n$"
msg_tail:
    .ascii "TAIL\r\n$"
