.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * DIR.COM — classic-style listing via INT 21h FindFirst/Next.
 * Args from PSP command tail (81h). Same rules as COMMAND.COM DIR.
 */

_start:
    push cs
    pop ds
    push cs
    pop es

    mov word ptr [dir_count], 0
    mov word ptr [dir_bytes], 0
    mov word ptr [dir_bytes + 2], 0

    mov si, 0x81
    call skip_spaces_psp
    call dir_build_pattern

    mov ah, 0x1A
    lea dx, [dta]
    int 0x21

    call dir_print_header

    mov ah, 0x4E
    lea dx, [dirpat]
    mov cx, 0x10
    int 0x21
    jc .dir_none

.dir_main:
    call dir_print_entry
    mov ah, 0x4F
    int 0x21
    jnc .dir_main

    call dir_print_footer
    mov ax, 0x4C00
    int 0x21

.dir_none:
    mov ah, 0x09
    lea dx, [msg_nf]
    int 0x21
    mov ax, 0x4C01
    int 0x21

skip_spaces_psp:
.ssp_loop:
    mov al, [si]
    cmp al, ' '
    je .ssp_inc
    cmp al, 0x0D
    je .ssp_z
    cmp al, 0
    je .ssp_z
    ret
.ssp_inc:
    inc si
    jmp .ssp_loop
.ssp_z:
    mov byte ptr [si], 0
    ret

dir_build_pattern:
    push ax
    push di
    lea di, [dirpat]
    mov al, [si]
    test al, al
    jz .dbp_all
    cmp al, 0x0D
    je .dbp_all
.dbp_copy:
    mov al, [si]
    test al, al
    jz .dbp_copied
    cmp al, 0x0D
    je .dbp_copied
    cmp al, ' '
    je .dbp_copied
    call up_al
    stosb
    inc si
    jmp .dbp_copy
.dbp_copied:
    mov byte ptr [di], 0
    lea di, [dirpat]
.dbp_scan:
    mov al, [di]
    test al, al
    jz .dbp_isdir
    cmp al, '*'
    je .dbp_done
    cmp al, '?'
    je .dbp_done
    cmp al, '.'
    je .dbp_done
    inc di
    jmp .dbp_scan
.dbp_isdir:
    mov byte ptr [di], '\\'
    inc di
    mov byte ptr [di], '*'
    inc di
    mov byte ptr [di], '.'
    inc di
    mov byte ptr [di], '*'
    inc di
    mov byte ptr [di], 0
    jmp .dbp_done
.dbp_all:
    mov byte ptr [di], '*'
    inc di
    mov byte ptr [di], '.'
    inc di
    mov byte ptr [di], '*'
    inc di
    mov byte ptr [di], 0
.dbp_done:
    pop di
    pop ax
    ret

up_al:
    cmp al, 'a'
    jb .ua
    cmp al, 'z'
    ja .ua
    sub al, 0x20
.ua:
    ret

dir_print_header:
    push ax
    push dx
    push si
    push di
    mov ah, 0x09
    lea dx, [msg_hdr]
    int 0x21
    lea si, [dirpat]
    xor di, di
.dph_scan:
    mov al, [si]
    test al, al
    jz .dph_scanned
    cmp al, '\\'
    jne .dph_n
    mov di, si
.dph_n:
    inc si
    jmp .dph_scan
.dph_scanned:
    test di, di
    jz .dph_cwd
    lea si, [dirpat]
.dph_comp:
    cmp si, di
    jae .dph_nl
    mov dl, [si]
    mov ah, 0x02
    int 0x21
    inc si
    jmp .dph_comp
.dph_cwd:
    lea si, [cwd_tmp]
    mov ah, 0x47
    mov dl, 0
    int 0x21
    lea si, [cwd_tmp]
    cmp byte ptr [si], 0
    je .dph_nl
.dph_c:
    lodsb
    test al, al
    jz .dph_nl
    mov dl, al
    mov ah, 0x02
    int 0x21
    jmp .dph_c
.dph_nl:
    mov ah, 0x09
    lea dx, [msg_crlf]
    int 0x21
    pop di
    pop si
    pop dx
    pop ax
    ret

dir_print_entry:
    push ax
    push bx
    push cx
    push dx
    push si
    lea si, [dta + 0x1E]
    xor cx, cx
.dpe_name:
    lodsb
    test al, al
    jz .dpe_pad
    mov dl, al
    mov ah, 0x02
    int 0x21
    inc cx
    jmp .dpe_name
.dpe_pad:
    cmp cx, 13
    jae .dpe_attr
    mov dl, ' '
    mov ah, 0x02
    int 0x21
    inc cx
    jmp .dpe_pad
.dpe_attr:
    test byte ptr [dta + 0x15], 0x10
    jz .dpe_file
    mov ah, 0x09
    lea dx, [msg_tag]
    int 0x21
    jmp .dpe_nl
.dpe_file:
    mov ax, [dta + 0x1A]
    mov dx, [dta + 0x1C]
    call print_u32
.dpe_nl:
    mov ah, 0x09
    lea dx, [msg_crlf]
    int 0x21
    lea si, [dta + 0x1E]
    cmp byte ptr [si], '.'
    jne .dpe_count
    cmp byte ptr [si + 1], 0
    je .dpe_done
    cmp byte ptr [si + 1], '.'
    jne .dpe_count
    cmp byte ptr [si + 2], 0
    je .dpe_done
.dpe_count:
    inc word ptr [dir_count]
    test byte ptr [dta + 0x15], 0x10
    jnz .dpe_done
    mov ax, [dta + 0x1A]
    add [dir_bytes], ax
    mov ax, [dta + 0x1C]
    adc [dir_bytes + 2], ax
.dpe_done:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

dir_print_footer:
    push ax
    push bx
    push cx
    push dx
    mov ah, 0x09
    lea dx, [msg_fs1]
    int 0x21
    mov ax, [dir_count]
    xor dx, dx
    call print_u32
    mov ah, 0x09
    lea dx, [msg_fs2]
    int 0x21
    mov ax, [dir_bytes]
    mov dx, [dir_bytes + 2]
    call print_u32
    mov ah, 0x09
    lea dx, [msg_fs3]
    int 0x21
    mov ah, 0x36
    mov dl, 0
    int 0x21
    push dx
    mul cx
    mov bx, ax
    pop ax
    mul bx
    call print_u32
    mov ah, 0x09
    lea dx, [msg_fs4]
    int 0x21
    pop dx
    pop cx
    pop bx
    pop ax
    ret

print_u32:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    lea di, [numbuf + 10]
    mov byte ptr [di], 0
    mov bx, 10
    mov si, ax
    mov cx, dx
.pu_loop:
    xor dx, dx
    mov ax, cx
    div bx
    mov cx, ax
    mov ax, si
    div bx
    mov si, ax
    add dl, '0'
    dec di
    mov [di], dl
    mov ax, cx
    or ax, si
    jnz .pu_loop
.pu_out:
    mov al, [di]
    test al, al
    jz .pu_done
    mov dl, al
    mov ah, 0x02
    int 0x21
    inc di
    jmp .pu_out
.pu_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

msg_hdr:
    .ascii " Directory of A:\\$"
msg_tag:
    .ascii "<DIR>$"
msg_fs1:
    .ascii "        $"
msg_fs2:
    .ascii " File(s)     $"
msg_fs3:
    .ascii " bytes\r\n                    $"
msg_fs4:
    .ascii " bytes free\r\n$"
msg_nf:
    .ascii "File not found\r\n$"
msg_crlf:
    .ascii "\r\n$"
dirpat:
    .space 64, 0
cwd_tmp:
    .space 64, 0
numbuf:
    .space 12, 0
dir_count:
    .word 0
dir_bytes:
    .word 0, 0
dta:
    .space 128, 0
