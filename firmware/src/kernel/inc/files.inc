/*
 * INT 21h file + device handles (FAT12 + CON/NUL).
 *
 * Entry (16 bytes):
 *   +0  used: 0 free, 1 file, 2 CON, 3 NUL
 *   +1  mode: 0=R 1=W 2=RW
 *   +2  start_cluster
 *   +4  file_size_lo / +6 size_hi
 *   +8  file_pos_lo  / +10 pos_hi
 *   +12 dir_lba / +14 dir_idx
 */

.equ HANDLE_SIZE, 16
.equ MAX_HANDLES, 8
.equ H_FILE, 1
.equ H_CON, 2
.equ H_NUL, 3

handle_index:
    cmp bx, MAX_HANDLES
    jae .hi_bad
    mov ax, bx
    mov cl, 4
    shl ax, cl
    lea di, [handles]
    add di, ax
    clc
    ret
.hi_bad:
    stc
    ret

dos_open_slot:
    push cs
    pop es
    lea di, [handles]
    xor bx, bx
.dos_scan:
    cmp bx, MAX_HANDLES
    jae .dos_full
    cmp byte ptr es:[di], 0
    je .dos_ok
    add di, HANDLE_SIZE
    inc bx
    jmp .dos_scan
.dos_full:
    stc
    ret
.dos_ok:
    clc
    ret

/* AH=3Dh open */
dos_ah3d:
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push bp

    mov bp, ax
    mov si, dx
    call is_device_name
    jc .a3d_file
    call dos_open_slot
    jc .a3d_err
    mov byte ptr es:[di], dl
    mov byte ptr es:[di + 1], 2
    mov ax, bx
    jmp .a3d_ok

.a3d_file:
    call path_resolve
    jc .a3d_err
    call path_name83_present
    jc .a3d_err
    mov dx, word ptr cs:[path_resolve_cluster]
    call fat12_find_in_dir
    jc .a3d_err
    /* AX=cluster, CX=size_lo, BX=size_hi */
    push ax
    push cx
    push bx
    call dos_open_slot
    jc .a3d_err_pop3
    mov word ptr cs:[xfer_cnt], bx
    pop dx
    pop cx
    pop ax
    mov byte ptr es:[di], H_FILE
    push ax
    mov ax, bp
    mov byte ptr es:[di + 1], al
    pop ax
    mov word ptr es:[di + 2], ax
    mov word ptr es:[di + 4], cx
    mov word ptr es:[di + 6], dx
    xor ax, ax
    mov word ptr es:[di + 8], ax
    mov word ptr es:[di + 10], ax
    mov ax, word ptr cs:[last_dir_lba]
    mov word ptr es:[di + 12], ax
    mov ax, word ptr cs:[last_dir_idx]
    mov word ptr es:[di + 14], ax
    mov ax, word ptr cs:[xfer_cnt]
    jmp .a3d_ok

.a3d_err_pop3:
    add sp, 6
.a3d_err:
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    mov ax, 2
    stc
    jmp int21_iret_cf

.a3d_ok:
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    /* AX = handle; also mirror into BX for older rmDOS tools */
    mov bx, ax
    clc
    jmp int21_iret

is_device_name:
    push ax
    push si
    mov al, [si]
    cmp al, 'C'
    je .idn_c
    cmp al, 'c'
    je .idn_c
    cmp al, 'N'
    je .idn_n
    cmp al, 'n'
    je .idn_n
.idn_file:
    pop si
    pop ax
    stc
    ret
.idn_c:
    cmp byte ptr [si + 1], 'O'
    je .idn_co
    cmp byte ptr [si + 1], 'o'
    jne .idn_file
.idn_co:
    cmp byte ptr [si + 2], 'N'
    je .idn_con
    cmp byte ptr [si + 2], 'n'
    jne .idn_file
.idn_con:
    mov al, [si + 3]
    test al, al
    jz .idn_is_con
    cmp al, ':'
    je .idn_is_con
    jmp .idn_file
.idn_is_con:
    mov dl, H_CON
    pop si
    pop ax
    clc
    ret
.idn_n:
    cmp byte ptr [si + 1], 'U'
    je .idn_nu
    cmp byte ptr [si + 1], 'u'
    jne .idn_file
.idn_nu:
    cmp byte ptr [si + 2], 'L'
    je .idn_nul
    cmp byte ptr [si + 2], 'l'
    jne .idn_file
.idn_nul:
    mov al, [si + 3]
    test al, al
    jz .idn_is_nul
    cmp al, ':'
    je .idn_is_nul
    jmp .idn_file
.idn_is_nul:
    mov dl, H_NUL
    pop si
    pop ax
    clc
    ret

/* AH=3Ch create */
dos_ah3c:
    push cx
    push dx
    push si
    push di
    push ds
    push es

    call path_resolve
    jc .a3c_err
    call path_name83_present
    jc .a3c_err
    mov dx, word ptr cs:[path_resolve_cluster]
    push dx
    call fat12_find_in_dir
    pop dx
    jnc .a3c_trunc

    call fat12_find_free_in_dir
    jc .a3c_err
    mov al, 0x20
    call fat12_create_dirent_attr
    jc .a3c_err
    xor ax, ax
    xor cx, cx
    xor bx, bx
    jmp .a3c_open

.a3c_trunc:
    push ax
    call fat12_free_chain
    pop ax
    xor ax, ax
    xor dx, dx
    mov word ptr cs:[last_size_hi], 0
    call fat12_update_dirent
    jc .a3c_err
    call fat12_flush_fat
    jc .a3c_err
    xor ax, ax
    xor cx, cx
    xor bx, bx

.a3c_open:
    push ax
    push bx
    push cx
    call dos_open_slot
    jc .a3c_err_pop3
    mov word ptr cs:[xfer_cnt], bx
    pop cx
    pop dx
    pop ax
    mov byte ptr es:[di], H_FILE
    mov byte ptr es:[di + 1], 2
    mov word ptr es:[di + 2], ax
    mov word ptr es:[di + 4], cx
    mov word ptr es:[di + 6], dx
    xor ax, ax
    mov word ptr es:[di + 8], ax
    mov word ptr es:[di + 10], ax
    mov ax, word ptr cs:[last_dir_lba]
    mov word ptr es:[di + 12], ax
    mov ax, word ptr cs:[last_dir_idx]
    mov word ptr es:[di + 14], ax
    mov ax, word ptr cs:[xfer_cnt]
    mov bx, ax

    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    clc
    jmp int21_iret

.a3c_err_pop3:
    add sp, 6
    jmp .a3c_err

.a3c_err:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    mov ax, 3
    stc
    jmp int21_iret_cf

dos_ah3e:
    push ax
    push cx
    push dx
    push di
    push es
    push cs
    pop es
    call handle_index
    jc .a3e_err
    cmp byte ptr es:[di], 0
    je .a3e_err
    cmp byte ptr es:[di], H_FILE
    jne .a3e_clr
    mov ax, word ptr es:[di + 12]
    mov word ptr cs:[last_dir_lba], ax
    mov ax, word ptr es:[di + 14]
    mov word ptr cs:[last_dir_idx], ax
    mov ax, word ptr es:[di + 2]
    mov dx, word ptr es:[di + 4]
    mov cx, word ptr es:[di + 6]
    mov word ptr cs:[last_size_hi], cx
    call fat12_update_dirent
    call fat12_flush_fat
.a3e_clr:
    mov byte ptr es:[di], 0
    pop es
    pop di
    pop dx
    pop cx
    pop ax
    clc
    jmp int21_iret
.a3e_err:
    pop es
    pop di
    pop dx
    pop cx
    pop ax
    mov ax, 6
    stc
    jmp int21_iret_cf

/* AH=3Fh read — sector chunks */
dos_ah3f:
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push es
    push ds

    mov word ptr cs:[xfer_buf_off], dx
    mov word ptr cs:[xfer_buf_seg], ds
    mov word ptr cs:[xfer_want], cx

    push cs
    pop es
    call handle_index
    jc .a3f_fail
    mov al, es:[di]
    cmp al, H_NUL
    je .a3f_empty
    cmp al, H_CON
    je .a3f_con
    cmp al, H_FILE
    jne .a3f_fail
    mov si, di

    mov ax, es:[si + 4]
    mov dx, es:[si + 6]
    sub ax, es:[si + 8]
    sbb dx, es:[si + 10]
    jc .a3f_empty
    jnz .a3f_cap
    cmp ax, word ptr cs:[xfer_want]
    jae .a3f_cap
    mov word ptr cs:[xfer_want], ax
.a3f_cap:
    cmp word ptr cs:[xfer_want], 0
    je .a3f_empty

    xor bp, bp
.a3f_loop:
    cmp bp, word ptr cs:[xfer_want]
    jae .a3f_ok
    call file_cluster_at_pos
    jc .a3f_fail
    mov word ptr cs:[xfer_soff], cx
    push ax
    call fat12_cluster_to_lba
    push es
    push cs
    pop es
    lea bx, [sector_buf]
    call fat12_read_lba
    pop es
    pop ax
    jc .a3f_fail

    mov ax, 512
    sub ax, word ptr cs:[xfer_soff]
    mov bx, word ptr cs:[xfer_want]
    sub bx, bp
    cmp ax, bx
    jbe .a3f_n
    mov ax, bx
.a3f_n:
    mov word ptr cs:[xfer_cnt], ax
    push ds
    push es
    push si
    push di
    mov es, word ptr cs:[xfer_buf_seg]
    mov di, word ptr cs:[xfer_buf_off]
    add di, bp
    push cs
    pop ds
    lea si, [sector_buf]
    add si, word ptr cs:[xfer_soff]
    mov cx, word ptr cs:[xfer_cnt]
    rep movsb
    pop di
    pop si
    pop es
    pop ds

    mov ax, word ptr cs:[xfer_cnt]
    add bp, ax
    add word ptr es:[si + 8], ax
    adc word ptr es:[si + 10], 0
    jmp .a3f_loop

.a3f_con:
    mov ds, word ptr cs:[xfer_buf_seg]
    mov bx, word ptr cs:[xfer_buf_off]
    mov di, word ptr cs:[xfer_want]
    xor bp, bp
.a3f_ck:
    cmp bp, di
    jae .a3f_ok
    call check_break
    mov ah, 0
    int 0x16
    mov [bx], al
    call putch
    inc bx
    inc bp
    cmp al, 0x0D
    jne .a3f_ck
.a3f_ok:
    mov ax, bp
    jmp .a3f_ret
.a3f_empty:
    xor ax, ax
.a3f_ret:
    pop ds
    pop es
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    clc
    jmp int21_iret
.a3f_fail:
    pop ds
    pop es
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov ax, 6
    stc
    jmp int21_iret_cf

/* AH=40h write — sector chunks */
dos_ah40:
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push es
    push ds

    mov word ptr cs:[xfer_buf_off], dx
    mov word ptr cs:[xfer_buf_seg], ds
    mov word ptr cs:[xfer_want], cx

    push cs
    pop es
    call handle_index
    jc .a40_fail
    mov al, es:[di]
    cmp al, H_NUL
    je .a40_nul
    cmp al, H_CON
    je .a40_con
    cmp al, H_FILE
    jne .a40_fail
    cmp byte ptr es:[di + 1], 0
    je .a40_fail
    mov si, di
    cmp word ptr cs:[xfer_want], 0
    je .a40_empty

    xor bp, bp
.a40_loop:
    cmp bp, word ptr cs:[xfer_want]
    jae .a40_ok
    call file_cluster_at_pos
    jc .a40_fail
    mov word ptr cs:[xfer_soff], cx
    push ax
    call fat12_cluster_to_lba
    push es
    push cs
    pop es
    lea bx, [sector_buf]
    call fat12_read_lba
    pop es
    pop ax
    jc .a40_fail

    mov bx, 512
    sub bx, word ptr cs:[xfer_soff]
    mov cx, word ptr cs:[xfer_want]
    sub cx, bp
    cmp bx, cx
    jbe .a40_n
    mov bx, cx
.a40_n:
    mov word ptr cs:[xfer_cnt], bx

    push ds
    push es
    push si
    push di
    push ax
    mov ds, word ptr cs:[xfer_buf_seg]
    mov si, word ptr cs:[xfer_buf_off]
    add si, bp
    push cs
    pop es
    lea di, [sector_buf]
    add di, word ptr cs:[xfer_soff]
    mov cx, word ptr cs:[xfer_cnt]
    rep movsb
    pop ax
    pop di
    pop si
    pop es
    pop ds

    push ax
    call fat12_cluster_to_lba
    push es
    push cs
    pop es
    lea bx, [sector_buf]
    call fat12_write_lba
    pop es
    pop ax
    jc .a40_fail

    mov ax, word ptr cs:[xfer_cnt]
    add bp, ax
    add word ptr es:[si + 8], ax
    adc word ptr es:[si + 10], 0
    mov ax, es:[si + 8]
    mov dx, es:[si + 10]
    cmp dx, es:[si + 6]
    ja .a40_grow
    jb .a40_nosz
    cmp ax, es:[si + 4]
    jbe .a40_nosz
.a40_grow:
    mov es:[si + 4], ax
    mov es:[si + 6], dx
.a40_nosz:
    jmp .a40_loop

.a40_con:
    mov ds, word ptr cs:[xfer_buf_seg]
    mov bx, word ptr cs:[xfer_buf_off]
    mov di, word ptr cs:[xfer_want]
    xor bp, bp
.a40_ck:
    cmp bp, di
    jae .a40_ok
    mov al, [bx]
    call putch
    inc bx
    inc bp
    jmp .a40_ck
.a40_nul:
    mov bp, word ptr cs:[xfer_want]
    jmp .a40_ok
.a40_empty:
    xor bp, bp
.a40_ok:
    mov ax, bp
    pop ds
    pop es
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    clc
    jmp int21_iret
.a40_fail:
    pop ds
    pop es
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov ax, 5
    stc
    jmp int21_iret_cf

/* ES:SI handle → AX=cluster, CX=byte offset in sector */
file_cluster_at_pos:
    push bx
    push dx
    push di
    mov ax, es:[si + 2]
    test ax, ax
    jnz .fcp_have
    call fat12_alloc_cluster
    jc .fcp_err
    mov es:[si + 2], ax
.fcp_have:
    mov ax, es:[si + 8]
    mov dx, es:[si + 10]
    mov cx, 512
    div cx
    mov word ptr cs:[xfer_soff], dx
    mov di, ax
    mov ax, es:[si + 2]
    xor bx, bx
.fcp_loop:
    cmp bx, di
    jae .fcp_ok
    mov dx, ax
    call fat12_next_cluster
    cmp ax, FAT12_EOC
    jb .fcp_next
    push bx
    push cx
    push dx
    call fat12_alloc_cluster
    mov bx, ax
    pop ax
    jc .fcp_xf
    mov dx, bx
    call fat12_set_fat
    mov ax, bx
    pop cx
    pop bx
    jmp .fcp_next
.fcp_xf:
    pop cx
    pop bx
    jmp .fcp_err
.fcp_next:
    inc bx
    jmp .fcp_loop
.fcp_ok:
    mov cx, word ptr cs:[xfer_soff]
    pop di
    pop dx
    pop bx
    clc
    ret
.fcp_err:
    pop di
    pop dx
    pop bx
    stc
    ret

/* AH=41h delete */
dos_ah41:
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

    call path_resolve
    jc .a41_err
    call path_name83_present
    jc .a41_err
    mov dx, word ptr cs:[path_resolve_cluster]
    call fat12_find_in_dir
    jc .a41_err
    push ax
    mov ax, word ptr cs:[last_dir_lba]
    push cs
    pop es
    lea bx, [sector_buf]
    call fat12_read_lba
    mov bx, word ptr cs:[last_dir_idx]
    mov cl, 5
    shl bx, cl
    lea di, [sector_buf]
    add di, bx
    test byte ptr es:[di + 11], 0x10
    pop ax
    jnz .a41_err
    call fat12_free_chain
    call fat12_delete_dirent
    jc .a41_err
    call fat12_flush_fat
    jc .a41_err

    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    xor ax, ax
    clc
    jmp int21_iret

.a41_err:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov ax, 2
    stc
    jmp int21_iret_cf

/* AH=42h seek CX:DX */
dos_ah42:
    push bx
    push si
    push di
    push es
    push cs
    pop es
    call handle_index
    jc .a42_err
    cmp byte ptr es:[di], H_FILE
    jne .a42_err
    mov bx, dx
    mov si, cx
    cmp al, 0
    je .a42_set
    cmp al, 1
    je .a42_cur
    cmp al, 2
    je .a42_end
    jmp .a42_err
.a42_cur:
    add bx, word ptr es:[di + 8]
    adc si, word ptr es:[di + 10]
    jmp .a42_set
.a42_end:
    add bx, word ptr es:[di + 4]
    adc si, word ptr es:[di + 6]
.a42_set:
    mov word ptr es:[di + 8], bx
    mov word ptr es:[di + 10], si
    mov ax, bx
    mov dx, si
    pop es
    pop di
    pop si
    pop bx
    clc
    jmp int21_iret
.a42_err:
    pop es
    pop di
    pop si
    pop bx
    mov ax, 1
    stc
    jmp int21_iret_cf

dos_ah44:
    cmp al, 0
    jne .a44_bad
    push di
    push es
    push cs
    pop es
    call handle_index
    jc .a44_err
    mov al, es:[di]
    cmp al, H_FILE
    je .a44_file
    mov dx, 0x80C3
    cmp al, H_NUL
    jne .a44_dev
    mov dx, 0x80C4
.a44_dev:
    pop es
    pop di
    xor ax, ax
    clc
    jmp int21_iret
.a44_file:
    xor dx, dx
    pop es
    pop di
    xor ax, ax
    clc
    jmp int21_iret
.a44_err:
    pop es
    pop di
.a44_bad:
    mov ax, 1
    stc
    jmp int21_iret_cf

init_std_handles:
    push es
    push di
    push cs
    pop es
    lea di, [handles]
    mov byte ptr es:[di], H_CON
    mov byte ptr es:[di + HANDLE_SIZE], H_CON
    mov byte ptr es:[di + HANDLE_SIZE * 2], H_CON
    pop di
    pop es
    ret
