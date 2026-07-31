/*
 * INT 21h AH=1Ah / 4Eh / 4Fh — DTA + find.
 *
 * DTA: +00 dir_cluster, +02 sector_index, +04 entry_idx, +05 search_attr
 *      +15 attr … +1E name
 * Pattern in CS:find_pat.
 *
 * AH=4Eh accepts cwd-relative or absolute paths with a final wildcard
 * component, e.g. DIR\*.*, \DIR\FILE.TXT, *.*
 */

dos_ah1a:
    mov word ptr cs:[dta_off], dx
    mov word ptr cs:[dta_seg], ds
    clc
    jmp int21_iret

dos_ah4e:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds

    mov byte ptr cs:[find_attr], cl
    call find_split_path
    /* DX = dir cluster, DS:SI = pattern ASCIZ */
    jc .a4e_fail
    call find_parse_pattern

    mov es, word ptr cs:[dta_seg]
    mov di, word ptr cs:[dta_off]
    mov word ptr es:[di], dx
    mov word ptr es:[di + 2], 0
    mov byte ptr es:[di + 4], 0
    mov al, byte ptr cs:[find_attr]
    mov byte ptr es:[di + 5], al

    push cs
    pop ds
    call find_search
    jc .a4e_fail

    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    xor ax, ax
    clc
    jmp int21_iret

.a4e_fail:
    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov ax, 0x12
    stc
    jmp int21_iret_cf

dos_ah4f:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds
    push cs
    pop ds
    call find_search
    jc .a4f_fail
    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    xor ax, ax
    clc
    jmp int21_iret
.a4f_fail:
    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov ax, 0x12
    stc
    jmp int21_iret_cf

/*
 * Split DS:DX path into directory cluster + pattern.
 * OUT: DX = dir cluster, DS:SI = pattern; CF on bad directory.
 * Uses CS:find_dirbuf for a mutable copy.
 */
find_split_path:
    push ax
    push bx
    push cx
    push di
    push es

    mov si, dx
    cmp byte ptr [si + 1], ':'
    jne .fsp_copy
    add si, 2
.fsp_copy:
    /* copy path to CS:find_dirbuf (ASCIZ, max 63) */
    push cs
    pop es
    lea di, [find_dirbuf]
    mov cx, 63
.fsp_cp:
    lodsb
    stosb
    test al, al
    jz .fsp_cp_done
    loop .fsp_cp
    mov byte ptr es:[di], 0
.fsp_cp_done:

    push cs
    pop ds
    lea si, [find_dirbuf]
    /* BX = offset of char after last separator (0 = none) */
    xor bx, bx
    mov di, si
.fsp_scan:
    mov al, [di]
    test al, al
    jz .fsp_scanned
    cmp al, '\\'
    je .fsp_sep
    cmp al, '/'
    je .fsp_sep
    inc di
    jmp .fsp_scan
.fsp_sep:
    inc di
    mov bx, di
    jmp .fsp_scan
.fsp_scanned:
    test bx, bx
    jnz .fsp_has_sep

    /* no separator: cwd + whole string is pattern */
    mov dx, word ptr cs:[cwd_cluster]
    lea si, [find_dirbuf]
    clc
    jmp .fsp_out

.fsp_has_sep:
    /* pattern at BX; directory is prefix (empty → root) */
    mov byte ptr [bx - 1], 0
    lea si, [find_dirbuf]
    cmp byte ptr [si], 0
    jne .fsp_resolve_dir
    /* "\pattern" → root */
    xor dx, dx
    mov si, bx
    clc
    jmp .fsp_out

.fsp_resolve_dir:
    /* path_resolve on directory ASCIZ at DS:DX */
    push bx
    lea dx, [find_dirbuf]
    call path_resolve
    pop bx
    jc .fsp_err
    call path_name83_present
    jc .fsp_err
    /* look up final component as directory */
    push bx
    call fat12_find_in_dir
    pop bx
    jc .fsp_err
    /* AX = start cluster of directory */
    push bx
    push ax
    call path_read_attr
    pop dx
    pop bx
    jc .fsp_err
    test al, 0x10
    jz .fsp_err
    mov si, bx
    clc
    jmp .fsp_out

.fsp_err:
    stc
.fsp_out:
    pop es
    pop di
    pop cx
    pop bx
    pop ax
    ret

find_parse_pattern:
    push ax
    push cx
    push di
    push es
    push si
    push cs
    pop es
    lea di, [find_pat]
    mov cx, 11
    mov al, '?'
    rep stosb
    cmp byte ptr [si], 0
    je .fpp_out
    lea di, [find_pat]
    mov cx, 8
.fpp_s:
    mov al, [si]
    test al, al
    jz .fpp_out
    cmp al, '.'
    je .fpp_d
    cmp al, '*'
    je .fpp_star_n
    inc si
    call find_up
    stosb
    loop .fpp_s
    cmp byte ptr [si], '.'
    jne .fpp_out
.fpp_d:
    inc si
    lea di, [find_pat + 8]
    mov cx, 3
.fpp_e:
    mov al, [si]
    test al, al
    jz .fpp_out
    cmp al, '*'
    je .fpp_out
    inc si
    call find_up
    stosb
    loop .fpp_e
    jmp .fpp_out
.fpp_star_n:
.fpp_sn:
    inc si
    mov al, [si]
    test al, al
    jz .fpp_out
    cmp al, '.'
    jne .fpp_sn
    jmp .fpp_d
.fpp_out:
    pop si
    pop es
    pop di
    pop cx
    pop ax
    ret

find_up:
    cmp al, 'a'
    jb .fu
    cmp al, 'z'
    ja .fu
    sub al, 0x20
.fu:
    ret

find_search:
    push ax
    push bx
    push cx
    push dx
    push si
    push bp

.fs_next:
    mov es, word ptr cs:[dta_seg]
    mov di, word ptr cs:[dta_off]
    mov dx, es:[di]
    mov bx, es:[di + 2]
    call fat12_dir_sector
    jc .fs_fail
    push cs
    pop es
    lea bx, [sector_buf]
    xor dx, dx
    call fat12_read_lba
    jc .fs_fail

    mov es, word ptr cs:[dta_seg]
    mov di, word ptr cs:[dta_off]
    mov bl, es:[di + 4]
    cmp bl, 16
    jb .fs_use
    inc word ptr es:[di + 2]
    mov byte ptr es:[di + 4], 0
    jmp .fs_next

.fs_use:
    xor bh, bh
    mov bp, bx
    mov ax, bp
    mov cl, 5
    shl ax, cl
    lea si, [sector_buf]
    add si, ax

    mov al, cs:[si]
    test al, al
    jz .fs_fail
    cmp al, 0xE5
    je .fs_skip
    test byte ptr cs:[si + 11], 0x08
    jnz .fs_skip

    mov al, cs:[si + 11]
    mov ah, es:[di + 5]
    test ah, 0x10
    jnz .fs_cmp
    test al, 0x10
    jnz .fs_skip

.fs_cmp:
    lea bx, [find_pat]
    push si
    mov cx, 11
.fs_c:
    mov al, cs:[bx]
    cmp al, '?'
    je .fs_ceq
    cmp al, cs:[si]
    jne .fs_cno
.fs_ceq:
    inc bx
    inc si
    loop .fs_c
    pop si
    jmp .fs_hit
.fs_cno:
    pop si
.fs_skip:
    mov es, word ptr cs:[dta_seg]
    mov di, word ptr cs:[dta_off]
    inc byte ptr es:[di + 4]
    jmp .fs_next

.fs_fail:
    stc
    jmp .fs_done

.fs_hit:
    mov es, word ptr cs:[dta_seg]
    mov di, word ptr cs:[dta_off]
    mov al, cs:[si + 11]
    mov es:[di + 0x15], al
    mov ax, cs:[si + 22]
    mov es:[di + 0x16], ax
    mov ax, cs:[si + 24]
    mov es:[di + 0x18], ax
    mov ax, cs:[si + 28]
    mov es:[di + 0x1A], ax
    mov ax, cs:[si + 30]
    mov es:[di + 0x1C], ax
    push di
    add di, 0x1E
    call find_fmt_name
    pop di
    inc byte ptr es:[di + 4]
    clc
.fs_done:
    pop bp
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

find_fmt_name:
    push ax
    push cx
    push si
    mov cx, 8
.ff_s:
    mov al, cs:[si]
    cmp al, ' '
    je .ff_se
    mov es:[di], al
    inc di
    inc si
    loop .ff_s
    jmp .ff_ext
.ff_se:
    add si, cx
.ff_ext:
    cmp byte ptr cs:[si], ' '
    je .ff_z
    mov byte ptr es:[di], '.'
    inc di
    mov cx, 3
.ff_e:
    mov al, cs:[si]
    cmp al, ' '
    je .ff_z
    mov es:[di], al
    inc di
    inc si
    loop .ff_e
.ff_z:
    mov byte ptr es:[di], 0
    pop si
    pop cx
    pop ax
    ret
