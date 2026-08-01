.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * INT21X.COM — gate new INT 21h / INT 2Fh APIs.
 * Prints FCB OK / TEMP OK / PSP OK / IOCTL OK then INT21X OK.
 */

_start:
    push cs
    pop ds

    /* --- FCB create / random block write+read / rename / delete --- */
    mov ah, 0x1A
    lea dx, [dta]
    int 0x21

    lea di, [fcb]
    mov cx, 40
    xor al, al
    rep stosb
    lea di, [fcb + 1]
    mov cx, 11
    mov al, ' '
    rep stosb
    mov byte ptr [fcb], 0
    mov byte ptr [fcb + 1], 'F'
    mov byte ptr [fcb + 2], 'C'
    mov byte ptr [fcb + 3], 'B'
    mov byte ptr [fcb + 4], 'T'
    mov byte ptr [fcb + 5], 'S'
    mov byte ptr [fcb + 6], 'T'
    mov byte ptr [fcb + 9], 'D'
    mov byte ptr [fcb + 10], 'A'
    mov byte ptr [fcb + 11], 'T'
    mov word ptr [fcb + 0x0E], 4

    mov ah, 0x16
    lea dx, [fcb]
    int 0x21
    cmp al, 0
    je .fcb_c1
    jmp fail_fcb
.fcb_c1:

    /* random block write 1 record "RM!!" at relrec 0 */
    mov byte ptr [dta], 'R'
    mov byte ptr [dta + 1], 'M'
    mov byte ptr [dta + 2], '!'
    mov byte ptr [dta + 3], '!'
    mov word ptr [fcb + 0x21], 0
    mov byte ptr [fcb + 0x23], 0
    mov cx, 1
    mov ah, 0x28
    lea dx, [fcb]
    int 0x21
    cmp al, 0
    jne fail_fcb
    cmp cx, 1
    jne fail_fcb

    /* random block read back */
    mov byte ptr [dta], 0
    mov byte ptr [dta + 1], 0
    mov byte ptr [dta + 2], 0
    mov byte ptr [dta + 3], 0
    mov word ptr [fcb + 0x21], 0
    mov byte ptr [fcb + 0x23], 0
    mov cx, 1
    mov ah, 0x27
    lea dx, [fcb]
    int 0x21
    cmp al, 0
    jne fail_fcb
    cmp byte ptr [dta], 'R'
    jne fail_fcb
    cmp byte ptr [dta + 1], 'M'
    jne fail_fcb

    mov ah, 0x10
    lea dx, [fcb]
    int 0x21
    cmp al, 0
    jne fail_fcb

    /* rename FCBTST.DAT -> FCBNEW.DAT (new name at FCB+11h) */
    lea di, [fcb]
    mov cx, 40
    xor al, al
    rep stosb
    lea di, [fcb + 1]
    mov cx, 11
    mov al, ' '
    rep stosb
    mov byte ptr [fcb + 1], 'F'
    mov byte ptr [fcb + 2], 'C'
    mov byte ptr [fcb + 3], 'B'
    mov byte ptr [fcb + 4], 'T'
    mov byte ptr [fcb + 5], 'S'
    mov byte ptr [fcb + 6], 'T'
    mov byte ptr [fcb + 9], 'D'
    mov byte ptr [fcb + 10], 'A'
    mov byte ptr [fcb + 11], 'T'
    lea di, [fcb + 0x11]
    mov cx, 11
    mov al, ' '
    rep stosb
    mov byte ptr [fcb + 0x11], 'F'
    mov byte ptr [fcb + 0x12], 'C'
    mov byte ptr [fcb + 0x13], 'B'
    mov byte ptr [fcb + 0x14], 'N'
    mov byte ptr [fcb + 0x15], 'E'
    mov byte ptr [fcb + 0x16], 'W'
    mov byte ptr [fcb + 0x19], 'D'
    mov byte ptr [fcb + 0x1A], 'A'
    mov byte ptr [fcb + 0x1B], 'T'
    mov ah, 0x17
    lea dx, [fcb]
    int 0x21
    cmp al, 0
    jne fail_fcb

    /* delete FCBNEW.DAT */
    lea di, [fcb]
    mov cx, 40
    xor al, al
    rep stosb
    lea di, [fcb + 1]
    mov cx, 11
    mov al, ' '
    rep stosb
    mov byte ptr [fcb + 1], 'F'
    mov byte ptr [fcb + 2], 'C'
    mov byte ptr [fcb + 3], 'B'
    mov byte ptr [fcb + 4], 'N'
    mov byte ptr [fcb + 5], 'E'
    mov byte ptr [fcb + 6], 'W'
    mov byte ptr [fcb + 9], 'D'
    mov byte ptr [fcb + 10], 'A'
    mov byte ptr [fcb + 11], 'T'
    mov ah, 0x13
    lea dx, [fcb]
    int 0x21
    cmp al, 0
    jne fail_fcb

    mov ah, 0x09
    lea dx, [msg_fcb]
    int 0x21

    /* --- TEMP --- */
    push cs
    pop ds
    xor cx, cx
    lea dx, [plain_tmp]
    mov ah, 0x3C
    int 0x21
    jnc .t_3c_ok
    jmp fail_temp
.t_3c_ok:
    mov [handle], ax
    mov ah, 0x3E
    mov bx, [handle]
    int 0x21
    mov ah, 0x41
    lea dx, [plain_tmp]
    int 0x21

    xor cx, cx
    lea dx, [plain_tmp]
    mov ah, 0x5B
    int 0x21
    jnc .t_5b_ok
    jmp fail_temp
.t_5b_ok:
    mov [handle], ax
    mov ah, 0x3E
    mov bx, [handle]
    int 0x21
    xor cx, cx
    lea dx, [plain_tmp]
    mov ah, 0x5B
    int 0x21
    jc .t_5b_exists
    jmp fail_temp
.t_5b_exists:
    mov ah, 0x41
    lea dx, [plain_tmp]
    int 0x21

    lea di, [tmpbuf]
    mov byte ptr [di], 0
    xor cx, cx
    lea dx, [tmpbuf]
    mov ah, 0x5A
    int 0x21
    jnc .t_5a_ok
    jmp fail_temp
.t_5a_ok:
    mov [handle], ax
    mov ah, 0x3E
    mov bx, [handle]
    int 0x21
    mov ah, 0x41
    lea dx, [tmpbuf]
    int 0x21

    mov ax, 0x5C00
    xor bx, bx
    xor cx, cx
    xor dx, dx
    xor si, si
    int 0x21
    jnc .t_5c_ok
    jmp fail_temp
.t_5c_ok:

    push cs
    pop ds
    push cs
    pop es
    lea si, [relname]
    lea di, [truebuf]
    mov ah, 0x60
    int 0x21
    jnc .t_60_cf
    jmp fail_temp
.t_60_cf:
    cmp byte ptr [truebuf], 'A'
    je .t_60_a
    jmp fail_temp
.t_60_a:
    cmp byte ptr [truebuf + 1], ':'
    je .t_60_ok
    jmp fail_temp
.t_60_ok:

    mov ax, 0x38FF
    mov bx, 1
    int 0x21
    jnc .t_38s
    jmp fail_temp
.t_38s:
    xor al, al
    mov ah, 0x38
    lea dx, [country_buf]
    int 0x21
    jnc .t_38g
    jmp fail_temp
.t_38g:
    cmp bx, 1
    je .t_38_ok
    jmp fail_temp
.t_38_ok:

    mov ah, 0x09
    lea dx, [msg_temp]
    int 0x21

    /* --- PSP 50/51/62 + AH=59 + AH=52 --- */
    mov ah, 0x51
    int 0x21
    mov [saved_psp], bx
    mov ah, 0x50
    int 0x21
    mov ah, 0x62
    int 0x21
    cmp bx, word ptr [saved_psp]
    je .psp_c1
    jmp fail_psp
.psp_c1:

    mov ax, 0x3D00
    lea dx, [nosuch]
    int 0x21
    jc .psp_c3
    jmp fail_psp
.psp_c3:
    mov ah, 0x59
    xor bx, bx
    int 0x21
    cmp ax, 2
    je .psp_c4
    jmp fail_psp
.psp_c4:

    mov ah, 0x52
    int 0x21
    mov ax, es
    test ax, ax
    jnz .psp_c5
    jmp fail_psp
.psp_c5:
    mov ax, es:[bx]
    test ax, ax
    jnz .psp_c6
    jmp fail_psp
.psp_c6:

    mov ah, 0x09
    lea dx, [msg_psp]
    int 0x21

    /* --- IOCTL 06 + INT 2F SHARE/APPEND --- */
    mov ax, 0x4406
    xor bx, bx
    int 0x21
    jnc .t_ioctl_cf
    jmp fail_ioctl
.t_ioctl_cf:
    cmp al, 0xFF
    je .t_ioctl_al
    jmp fail_ioctl
.t_ioctl_al:

    mov ax, 0x1000
    int 0x2F
    test al, al
    jz .t_share
    jmp fail_ioctl
.t_share:

    mov ax, 0xB700
    int 0x2F
    test al, al
    jz .t_append
    jmp fail_ioctl
.t_append:

    mov ah, 0x09
    lea dx, [msg_ioctl]
    int 0x21

    mov ah, 0x09
    lea dx, [msg_ok]
    int 0x21
    mov ax, 0x4C00
    int 0x21

fail_fcb:
    mov ah, 0x09
    lea dx, [msg_fcb_fail]
    int 0x21
    jmp fail_exit
fail_psp:
    mov ah, 0x09
    lea dx, [msg_psp_fail]
    int 0x21
    jmp fail_exit
fail_temp:
    mov ah, 0x09
    lea dx, [msg_temp_fail]
    int 0x21
    jmp fail_exit
fail_ioctl:
    mov ah, 0x09
    lea dx, [msg_ioctl_fail]
    int 0x21
fail_exit:
    mov ax, 0x4C01
    int 0x21

msg_fcb:
    .ascii "FCB OK\r\n$"
msg_psp:
    .ascii "PSP OK\r\n$"
msg_temp:
    .ascii "TEMP OK\r\n$"
msg_ioctl:
    .ascii "IOCTL OK\r\n$"
msg_ok:
    .ascii "INT21X OK\r\n$"
msg_fcb_fail:
    .ascii "FCB FAIL\r\n$"
msg_psp_fail:
    .ascii "PSP FAIL\r\n$"
msg_temp_fail:
    .ascii "TEMP FAIL\r\n$"
msg_ioctl_fail:
    .ascii "IOCTL FAIL\r\n$"
nosuch:
    .asciz "NOSUCH.XYZ"
relname:
    .asciz "FOO.TXT"
plain_tmp:
    .asciz "PLAIN.TMP"
saved_psp:
    .word 0
handle:
    .word 0
fcb:
    .space 64, 0
dta:
    .space 128, 0
tmpbuf:
    .space 64, 0
truebuf:
    .space 64, 0
country_buf:
    .space 40, 0
