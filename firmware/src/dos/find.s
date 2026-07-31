.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * FIND.COM — print lines containing a string.
 * Usage: FIND "string" file
 * Exit: 0 = match(es), 1 = none, 2 = error
 */

_start:
    push cs
    pop ds
    push cs
    pop es

    mov byte ptr [found], 0
    mov word ptr [linelen], 0

    mov si, 0x81
    call skip_spaces
    jc .usage

    /* needle: "..." or bare token */
    cmp byte ptr [si], '"'
    jne .bare_needle
    inc si
    lea di, [needle]
.copy_q:
    lodsb
    cmp al, '"'
    je .needle_done
    cmp al, 0x0D
    je .usage
    cmp al, 0
    je .usage
    stosb
    jmp .copy_q

.bare_needle:
    lea di, [needle]
.copy_b:
    lodsb
    cmp al, ' '
    je .needle_done
    cmp al, 0x0D
    je .usage
    cmp al, 0
    je .usage
    stosb
    jmp .copy_b

.needle_done:
    mov byte ptr [di], 0
    lea ax, [needle]
    cmp di, ax
    je .usage

    call skip_spaces
    jc .usage

    lea di, [pathbuf]
.copy_path:
    lodsb
    cmp al, ' '
    je .path_done
    cmp al, 0x0D
    je .path_done
    cmp al, 0
    je .path_done
    stosb
    jmp .copy_path
.path_done:
    mov byte ptr [di], 0
    lea ax, [pathbuf]
    cmp di, ax
    je .usage

    mov ah, 0x3D
    xor al, al
    lea dx, [pathbuf]
    int 0x21
    jc .err_open
    mov [handle], bx

.read:
    mov ah, 0x3F
    mov bx, [handle]
    mov cx, 1
    lea dx, [one]
    int 0x21
    jc .close_err
    test ax, ax
    jz .eof
    mov al, [one]
    cmp al, 0x0A
    je .end_line
    cmp al, 0x0D
    je .end_line_cr
    mov bx, [linelen]
    cmp bx, 126
    jae .read
    lea di, [linebuf]
    add di, bx
    mov [di], al
    inc word ptr [linelen]
    jmp .read

.end_line_cr:
    /* peek: drop following LF if present by reading next in loop after flush */
    call flush_line
    jmp .read

.end_line:
    call flush_line
    jmp .read

.eof:
    call flush_line
    mov ah, 0x3E
    mov bx, [handle]
    int 0x21
    mov al, 1
    cmp byte ptr [found], 0
    je .exit
    xor al, al
.exit:
    mov ah, 0x4C
    int 0x21

.close_err:
    mov ah, 0x3E
    mov bx, [handle]
    int 0x21
.err_open:
    mov ah, 0x09
    lea dx, [msg_err]
    int 0x21
    mov ax, 0x4C02
    int 0x21

.usage:
    mov ah, 0x09
    lea dx, [msg_usage]
    int 0x21
    mov ax, 0x4C02
    int 0x21

/* ---- helpers ---- */

skip_spaces:
    lodsb
    cmp al, ' '
    je skip_spaces
    cmp al, 0x09
    je skip_spaces
    cmp al, 0x0D
    je .ss_empty
    cmp al, 0
    je .ss_empty
    dec si
    clc
    ret
.ss_empty:
    stc
    ret

flush_line:
    mov bx, [linelen]
    test bx, bx
    jnz .fl_have
    /* empty line still "ends"; nothing to match */
    ret
.fl_have:
    lea di, [linebuf]
    add di, bx
    mov byte ptr [di], 0
    call line_has_needle
    jc .fl_clear
    mov byte ptr [found], 1
    /* print line + CRLF */
    lea si, [linebuf]
.fl_out:
    lodsb
    test al, al
    jz .fl_crlf
    mov dl, al
    mov ah, 0x02
    int 0x21
    jmp .fl_out
.fl_crlf:
    mov dl, 0x0D
    mov ah, 0x02
    int 0x21
    mov dl, 0x0A
    mov ah, 0x02
    int 0x21
.fl_clear:
    mov word ptr [linelen], 0
    ret

/* CF clear if needle found in linebuf (case-insensitive) */
line_has_needle:
    lea si, [linebuf]
.lh_outer:
    mov al, [si]
    test al, al
    jz .lh_miss
    push si
    lea di, [needle]
.lh_inner:
    mov al, [di]
    test al, al
    jz .lh_hit
    mov ah, [si]
    test ah, ah
    jz .lh_next
    call toupper_al
    xchg al, ah
    call toupper_al
    cmp al, ah
    jne .lh_next
    inc si
    inc di
    jmp .lh_inner
.lh_hit:
    pop si
    clc
    ret
.lh_next:
    pop si
    inc si
    jmp .lh_outer
.lh_miss:
    stc
    ret

toupper_al:
    cmp al, 'a'
    jb .tu_done
    cmp al, 'z'
    ja .tu_done
    sub al, 0x20
.tu_done:
    ret

msg_usage:
    .ascii "FIND \"string\" file\r\n$"
msg_err:
    .ascii "FIND: open failed\r\n$"
needle:
    .space 64, 0
pathbuf:
    .space 64, 0
linebuf:
    .space 128, 0
linelen:
    .word 0
handle:
    .word 0
one:
    .byte 0
found:
    .byte 0
