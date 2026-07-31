.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * MORE.COM — page text from a file (or stdin handle 0).
 * Usage: MORE [file]
 * Pauses every (screen_rows-1) lines with "-- More --".
 */

_start:
    push cs
    pop ds
    push cs
    pop es

    mov word ptr [line_count], 0
    call get_page_lines
    mov [page_lines], ax

    mov si, 0x81
    call skip_spaces
    jc .use_stdin

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
    je .use_stdin

    mov ah, 0x3D
    xor al, al
    lea dx, [pathbuf]
    int 0x21
    jc .err
    mov [handle], bx
    jmp .loop

.use_stdin:
    mov word ptr [handle], 0

.loop:
    mov ah, 0x3F
    mov bx, [handle]
    mov cx, 1
    lea dx, [one]
    int 0x21
    jc .done
    test ax, ax
    jz .done
    mov al, [one]
    mov dl, al
    mov ah, 0x02
    int 0x21
    cmp al, 0x0A
    jne .loop
    inc word ptr [line_count]
    mov ax, [line_count]
    cmp ax, [page_lines]
    jb .loop
    call pause_more
    mov word ptr [line_count], 0
    jmp .loop

.done:
    cmp word ptr [handle], 0
    je .exit0
    mov ah, 0x3E
    mov bx, [handle]
    int 0x21
.exit0:
    mov ax, 0x4C00
    int 0x21

.err:
    mov ah, 0x09
    lea dx, [msg_err]
    int 0x21
    mov ax, 0x4C01
    int 0x21

pause_more:
    mov ah, 0x09
    lea dx, [msg_more]
    int 0x21
    mov ah, 0x08
    int 0x21
    /* erase prompt: CR + spaces + CR */
    mov ah, 0x09
    lea dx, [msg_erase]
    int 0x21
    ret

get_page_lines:
    /* BDA 0040:0084 = rows-1; fall back to 24 */
    push es
    mov ax, 0x0040
    mov es, ax
    mov al, es:[0x84]
    pop es
    mov ah, 0
    test al, al
    jz .gpl_def
    /* AL is rows-1; page at that many lines */
    cmp al, 1
    jb .gpl_def
    ret
.gpl_def:
    mov ax, 24
    ret

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

msg_more:
    .ascii "-- More --$"
msg_erase:
    .ascii "\r          \r$"
msg_err:
    .ascii "MORE: open failed\r\n$"
pathbuf:
    .space 64, 0
handle:
    .word 0
one:
    .byte 0
line_count:
    .word 0
page_lines:
    .word 24
