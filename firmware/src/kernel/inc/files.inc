/*
 * INT 21h file + device handles (FAT12 + CON/NUL/AUX/PRN).
 *
 * Entry (16 bytes):
 *   +0  used: 0 free, 1 file, 2 CON, 3 NUL, 4 AUX, 5 PRN
 *   +1  mode: 0=R 1=W 2=RW
 *   +2  start_cluster
 *   +4  file_size_lo / +6 size_hi
 *   +8  file_pos_lo  / +10 pos_hi
 *   +12 dir_lba / +14 dir_idx (byte) / +15 drive (0=A)
 */

.equ HANDLE_SIZE, 16
.equ MAX_HANDLES_CAP, 64
.equ H_FILE, 1
.equ H_CON, 2
.equ H_NUL, 3
.equ H_AUX, 4
.equ H_PRN, 5

handle_index:
    cmp bx, word ptr cs:[max_handles]
    jae .hi_bad
    mov ax, bx
    /* ×16 without touching CX — AH=46 keeps the dest handle in CX */
    shl ax, 1
    shl ax, 1
    shl ax, 1
    shl ax, 1
    lea di, [handles]
    add di, ax
    clc
    ret
.hi_bad:
    stc
    ret

/*
 * Mirror cluster/size/pos from ES:SI to every other H_FILE handle that
 * refers to the same directory entry. Dup/force-dup copy entries rather
 * than sharing an SFT, so redirect (stdout→file) needs this to keep the
 * original create-handle in sync for a later close/flush.
 */
handles_mirror:
    push ax
    push bx
    push cx
    push di
    lea di, [handles]
    xor bx, bx
.hm_loop:
    cmp bx, word ptr cs:[max_handles]
    jae .hm_done
    cmp di, si
    je .hm_next
    cmp byte ptr es:[di], H_FILE
    jne .hm_next
    mov al, es:[di + 15]
    cmp al, es:[si + 15]
    jne .hm_next
    mov ax, es:[di + 12]
    cmp ax, es:[si + 12]
    jne .hm_next
    mov al, es:[di + 14]
    cmp al, es:[si + 14]
    jne .hm_next
    push si
    push di
    push ds
    push cs
    pop ds
    mov cx, 10                   /* +2 .. +11: cluster, size, pos */
    add si, 2
    add di, 2
    rep movsb
    pop ds
    pop di
    pop si
.hm_next:
    add di, HANDLE_SIZE
    inc bx
    jmp .hm_loop
.hm_done:
    pop di
    pop cx
    pop bx
    pop ax
    ret

/* Flush H_FILE at ES:DI to its directory entry (does not clear the slot). */
handle_flush_file:
    cmp byte ptr es:[di], H_FILE
    jne .hff_done
    push ax
    push cx
    push dx
    call handle_mount_drive
    jc .hff_pop
    mov ax, word ptr es:[di + 12]
    mov word ptr cs:[last_dir_lba], ax
    mov al, byte ptr es:[di + 14]
    xor ah, ah
    mov word ptr cs:[last_dir_idx], ax
    mov ax, word ptr es:[di + 2]
    mov dx, word ptr es:[di + 4]
    mov cx, word ptr es:[di + 6]
    mov word ptr cs:[last_size_hi], cx
    call fat12_update_dirent
    call fat12_flush_fat
.hff_pop:
    pop dx
    pop cx
    pop ax
.hff_done:
    ret

dos_open_slot:
    push cs
    pop es
    lea di, [handles]
    xor bx, bx
.dos_scan:
    cmp bx, word ptr cs:[max_handles]
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
    mov byte ptr es:[di + 14], al
    mov al, byte ptr cs:[path_resolve_drive]
    mov byte ptr es:[di + 15], al
    mov ax, word ptr cs:[xfer_cnt]
    jmp .a3d_ok

.a3d_err_pop3:
    add sp, 6
.a3d_err:
    call dos_remount_current
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
    call dos_remount_current
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
    call is_device_name_aux_check
    jnc .idn_ok_ret
    call is_device_name_prn_check
.idn_ok_ret:
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

/* AUX / COM1 */
is_device_name_aux_check:
    push si
    push ax
    mov al, [si]
    cmp al, 'A'
    je .idn_aux
    cmp al, 'a'
    je .idn_aux
    cmp al, 'C'
    je .idn_com
    cmp al, 'c'
    je .idn_com
    pop ax
    pop si
    stc
    ret
.idn_aux:
    cmp byte ptr [si + 1], 'U'
    je .idn_au
    cmp byte ptr [si + 1], 'u'
    jne .idn_aux_fail
.idn_au:
    cmp byte ptr [si + 2], 'X'
    je .idn_aux_ok
    cmp byte ptr [si + 2], 'x'
    jne .idn_aux_fail
.idn_aux_ok:
    mov al, [si + 3]
    test al, al
    jz .idn_is_aux
    cmp al, ':'
    je .idn_is_aux
.idn_aux_fail:
    pop ax
    pop si
    stc
    ret
.idn_is_aux:
    mov dl, H_AUX
    pop ax
    pop si
    clc
    ret
.idn_com:
    cmp byte ptr [si + 1], 'O'
    je .idn_co2
    cmp byte ptr [si + 1], 'o'
    jne .idn_aux_fail
.idn_co2:
    cmp byte ptr [si + 2], 'M'
    je .idn_com1
    cmp byte ptr [si + 2], 'm'
    jne .idn_aux_fail
.idn_com1:
    cmp byte ptr [si + 3], '1'
    jne .idn_aux_fail
    mov al, [si + 4]
    test al, al
    jz .idn_is_aux
    cmp al, ':'
    je .idn_is_aux
    jmp .idn_aux_fail

/* PRN / LPT1 — called after AUX fails */
is_device_name_prn_check:
    push si
    push ax
    mov al, [si]
    cmp al, 'P'
    je .idn_prn
    cmp al, 'p'
    je .idn_prn
    cmp al, 'L'
    je .idn_lpt
    cmp al, 'l'
    je .idn_lpt
    pop ax
    pop si
    stc
    ret
.idn_prn:
    cmp byte ptr [si + 1], 'R'
    je .idn_pr
    cmp byte ptr [si + 1], 'r'
    jne .idn_prn_fail
.idn_pr:
    cmp byte ptr [si + 2], 'N'
    je .idn_prn_ok
    cmp byte ptr [si + 2], 'n'
    jne .idn_prn_fail
.idn_prn_ok:
    mov al, [si + 3]
    test al, al
    jz .idn_is_prn
    cmp al, ':'
    je .idn_is_prn
.idn_prn_fail:
    pop ax
    pop si
    stc
    ret
.idn_is_prn:
    mov dl, H_PRN
    pop ax
    pop si
    clc
    ret
.idn_lpt:
    cmp byte ptr [si + 1], 'P'
    je .idn_lp
    cmp byte ptr [si + 1], 'p'
    jne .idn_prn_fail
.idn_lp:
    cmp byte ptr [si + 2], 'T'
    je .idn_lpt1
    cmp byte ptr [si + 2], 't'
    jne .idn_prn_fail
.idn_lpt1:
    cmp byte ptr [si + 3], '1'
    jne .idn_prn_fail
    mov al, [si + 4]
    test al, al
    jz .idn_is_prn
    cmp al, ':'
    je .idn_is_prn
    jmp .idn_prn_fail

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
    mov byte ptr es:[di + 14], al
    mov al, byte ptr cs:[path_resolve_drive]
    mov byte ptr es:[di + 15], al
    mov ax, word ptr cs:[xfer_cnt]
    mov bx, ax

    call dos_remount_current
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
    call dos_remount_current
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    mov ax, 3
    stc
    jmp int21_iret_cf

/* ES:DI = handle → mount its drive (file handles only). */
handle_mount_drive:
    push dx
    cmp byte ptr es:[di], H_FILE
    jne .hmd_ok
    mov dl, es:[di + 15]
    call dos_mount_drive
.hmd_ok:
    pop dx
    ret

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
    call handle_mount_drive
    jc .a3e_err
    mov ax, word ptr es:[di + 12]
    mov word ptr cs:[last_dir_lba], ax
    mov al, byte ptr es:[di + 14]
    xor ah, ah
    mov word ptr cs:[last_dir_idx], ax
    mov ax, word ptr es:[di + 2]
    mov dx, word ptr es:[di + 4]
    mov cx, word ptr es:[di + 6]
    mov word ptr cs:[last_size_hi], cx
    call fat12_update_dirent
    call fat12_flush_fat
.a3e_clr:
    mov byte ptr es:[di], 0
    call dos_remount_current
    pop es
    pop di
    pop dx
    pop cx
    pop ax
    clc
    jmp int21_iret
.a3e_err:
    call dos_remount_current
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
    cmp al, H_AUX
    je .a3f_aux
    cmp al, H_PRN
    je .a3f_fail                 /* PRN is write-only */
    cmp al, H_FILE
    jne .a3f_fail
    call handle_mount_drive
    jc .a3f_fail
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
    jae .a3f_file_done
    call file_cluster_at_pos
    jc .a3f_fail
    mov word ptr cs:[xfer_soff], cx
    push ax
    call fat12_cluster_to_lba
    add ax, cs:[xfer_sec]
    adc dx, 0
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
    /* Handle table lives in CS; do not use caller's ES (may be load seg). */
    add word ptr cs:[si + 8], ax
    adc word ptr cs:[si + 10], 0
    jmp .a3f_loop

.a3f_file_done:
    call handles_mirror
    jmp .a3f_ok

.a3f_con:
    mov ds, word ptr cs:[xfer_buf_seg]
    mov dx, word ptr cs:[xfer_buf_off]
    mov cx, word ptr cs:[xfer_want]
    mov bp, cx
    jcxz .a3f_ok
    call con_dev_read
    jmp .a3f_ok
.a3f_aux:
    mov ds, word ptr cs:[xfer_buf_seg]
    mov si, word ptr cs:[xfer_buf_off]
    mov cx, word ptr cs:[xfer_want]
    xor bp, bp
    jcxz .a3f_ok
.a3f_aux_lp:
    call aux_read_byte
    mov [si], al
    inc si
    inc bp
    loop .a3f_aux_lp
    jmp .a3f_ok
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
    cmp al, H_AUX
    je .a40_aux
    cmp al, H_PRN
    je .a40_prn
    cmp al, H_FILE
    jne .a40_fail
    cmp byte ptr es:[di + 1], 0
    je .a40_fail
    call handle_mount_drive
    jc .a40_fail
    mov si, di
    cmp word ptr cs:[xfer_want], 0
    je .a40_empty

    xor bp, bp
.a40_loop:
    cmp bp, word ptr cs:[xfer_want]
    jae .a40_file_done
    call file_cluster_at_pos
    jc .a40_fail
    mov word ptr cs:[xfer_soff], cx
    push ax
    call fat12_cluster_to_lba
    add ax, cs:[xfer_sec]
    adc dx, 0
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
    add ax, cs:[xfer_sec]
    adc dx, 0
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

.a40_file_done:
    call handles_mirror
    jmp .a40_ok

.a40_con:
    /* One OUTPUT with full length so ESC sequences stay contiguous for ANSI. */
    mov ds, word ptr cs:[xfer_buf_seg]
    mov dx, word ptr cs:[xfer_buf_off]
    mov cx, word ptr cs:[xfer_want]
    mov bp, cx
    jcxz .a40_ok
    call con_dev_write
    jmp .a40_ok
.a40_aux:
    mov ds, word ptr cs:[xfer_buf_seg]
    mov si, word ptr cs:[xfer_buf_off]
    mov cx, word ptr cs:[xfer_want]
    xor bp, bp
    jcxz .a40_ok
.a40_aux_lp:
    mov dl, [si]
    call aux_write_byte
    inc si
    inc bp
    loop .a40_aux_lp
    jmp .a40_ok
.a40_prn:
    mov ds, word ptr cs:[xfer_buf_seg]
    mov si, word ptr cs:[xfer_buf_off]
    mov cx, word ptr cs:[xfer_want]
    xor bp, bp
    jcxz .a40_ok
.a40_prn_lp:
    mov dl, [si]
    call prn_write_byte
    inc si
    inc bp
    loop .a40_prn_lp
    jmp .a40_ok
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

/* ES:SI handle → AX=cluster, CX=byte offset in sector; CS:xfer_sec = sector in cluster */
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
    div cx                       /* AX = sector index, DX = byte in sector */
    mov word ptr cs:[xfer_soff], dx
    mov bl, cs:[bpb_spc]
    xor bh, bh
    xor dx, dx
    div bx                       /* AX = cluster index, DX = sector in cluster */
    mov word ptr cs:[xfer_sec], dx
    mov di, ax
    mov ax, es:[si + 2]
    xor bx, bx
.fcp_loop:
    cmp bx, di
    jae .fcp_ok
    mov dx, ax
    call fat12_next_cluster
    cmp ax, cs:[fat_eoc]
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
    xor dx, dx
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

    call dos_remount_current
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
    call dos_remount_current
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
    push ax                     /* save AL = seek mode */
    push cs
    pop es
    call handle_index
    jc .a42_err_pop
    cmp byte ptr es:[di], H_FILE
    jne .a42_err_pop
    pop ax                      /* restore seek mode in AL */
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
    push si
    mov si, di
    call handles_mirror
    pop si
    mov ax, bx
    mov dx, si
    pop es
    pop di
    pop si
    pop bx
    clc
    jmp int21_iret
.a42_err_pop:
    pop ax
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
    je .a44_getinfo
    cmp al, 1
    je .a44_setinfo
    cmp al, 2
    je .a44_char_rd
    cmp al, 3
    je .a44_char_wr
    cmp al, 4
    je .a44_ctl_ok
    cmp al, 5
    je .a44_ctl_ok
    cmp al, 6
    je .a44_instat
    cmp al, 7
    je .a44_outstat
    cmp al, 8
    je .a44_remov
    cmp al, 0x0D
    je .a44_block
    jmp .a44_bad

.a44_getinfo:
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

/* AL=01 — set device info (accept for char devices / files) */
.a44_setinfo:
    push di
    push es
    push cs
    pop es
    call handle_index
    jc .a44_err
    /* DX already has new info; ignore stored value */
    pop es
    pop di
    xor ax, ax
    clc
    jmp int21_iret

/* AL=02/03 — char device control channel R/W (DS:DX, CX bytes) */
.a44_char_rd:
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es
    push cs
    pop es
    call handle_index
    jc .a44_c_err
    mov bp, es:[di]
    and bp, 0xFF                  /* BP = device type */
    cmp bp, H_FILE
    je .a44_c_bad
    mov si, dx                    /* DS:SI = caller buffer */
    xor bx, bx                    /* transferred */
    jcxz .a44_c_done
.a44_crd_loop:
    cmp bp, H_NUL
    je .a44_crd_nul
    cmp bp, H_AUX
    je .a44_crd_aux
    cmp bp, H_PRN
    je .a44_c_bad
    /* CON: 1-byte INPUT at DS:SI */
    push cx
    push dx
    mov dx, si
    mov cx, 1
    call con_dev_read
    pop dx
    pop cx
    jmp .a44_crd_next
.a44_crd_nul:
    mov byte ptr ds:[si], 0
    jmp .a44_crd_next
.a44_crd_aux:
    call aux_read_byte
    mov ds:[si], al
.a44_crd_next:
    inc si
    inc bx
    loop .a44_crd_loop
.a44_c_done:
    mov ax, bx
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    clc
    jmp int21_iret

.a44_char_wr:
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es
    push cs
    pop es
    call handle_index
    jc .a44_c_err
    mov bp, es:[di]
    and bp, 0xFF
    cmp bp, H_FILE
    je .a44_c_bad
    mov si, dx
    xor di, di
    jcxz .a44_cw_done
.a44_cwr_loop:
    mov al, ds:[si]
    cmp bp, H_NUL
    je .a44_cwr_next
    cmp bp, H_AUX
    je .a44_cwr_aux
    cmp bp, H_PRN
    je .a44_cwr_prn
    call putch
    jmp .a44_cwr_next
.a44_cwr_aux:
    mov dl, al
    call aux_write_byte
    jmp .a44_cwr_next
.a44_cwr_prn:
    mov dl, al
    call prn_write_byte
.a44_cwr_next:
    inc si
    inc di
    loop .a44_cwr_loop
.a44_cw_done:
    mov ax, di
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    clc
    jmp int21_iret

.a44_c_err:
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov ax, 6
    stc
    jmp int21_iret_cf
.a44_c_bad:
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov ax, 1
    stc
    jmp int21_iret_cf

/* AL=04/05 — control channel block/char stub: success AX=0 */
.a44_ctl_ok:
    xor ax, ax
    clc
    jmp int21_iret

/* AL=06 — get input status: AL=FF ready / 00 not */
.a44_instat:
    mov al, 0xFF
    clc
    jmp int21_iret

/* AL=07 — get output status: AL=FF ready / 00 not */
.a44_outstat:
    mov al, 0xFF
    clc
    jmp int21_iret

/* AL=08 — is drive removable? BL=drive (0=default) → AL=0 removable / 1 fixed */
.a44_remov:
    push bx
    mov bl, bl
    test bl, bl
    jnz .a44_rm_drv
    mov bl, byte ptr cs:[cur_drive]
    inc bl
.a44_rm_drv:
    dec bl
    cmp bl, 2
    jae .a44_fixed
    mov al, 0
    jmp .a44_rm_ok
.a44_fixed:
    mov al, 1
.a44_rm_ok:
    pop bx
    clc
    jmp int21_iret

/* AL=0D — block generic IOCTL stub: succeed with no-op */
.a44_block:
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

/* AH=45h — duplicate handle BX → AX */
dos_ah45:
    push cx
    push si
    push di
    push es
    push cs
    pop es
    call handle_index
    jc .a45_err
    cmp byte ptr es:[di], 0
    je .a45_err
    mov si, di
    call dos_open_slot
    jc .a45_err
    push ds
    push cs
    pop ds
    mov cx, HANDLE_SIZE
    rep movsb
    pop ds
    mov ax, bx
    pop es
    pop di
    pop si
    pop cx
    clc
    jmp int21_iret
.a45_err:
    pop es
    pop di
    pop si
    pop cx
    mov ax, 6
    stc
    jmp int21_iret_cf

/* AH=46h — force dup: make CX a copy of BX (close CX first) */
dos_ah46:
    push ax
    push bx
    push cx
    push si
    push di
    push es
    push cs
    pop es
    call handle_index
    jc .a46_err
    cmp byte ptr es:[di], 0
    je .a46_err
    mov si, di                   /* SI = source entry */
    push bx
    mov bx, cx
    call handle_index
    pop bx
    jc .a46_err
    /* flush+drop dest if it was an open file (redirect restore) */
    call handle_flush_file
    mov byte ptr es:[di], 0
    mov cx, HANDLE_SIZE
    push ds
    push cs
    pop ds
    rep movsb
    pop ds
    pop es
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    clc
    jmp int21_iret
.a46_err:
    pop es
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    mov ax, 6
    stc
    jmp int21_iret_cf

/* AH=57h — get/set file date/time */
dos_ah57:
    cmp al, 0
    je .a57_get
    cmp al, 1
    je .a57_set
    mov ax, 1
    stc
    jmp int21_iret_cf

.a57_get:
    push bx
    push si
    push di
    push es
    push ds
    push cs
    pop es
    call handle_index
    jc .a57_err
    cmp byte ptr es:[di], H_FILE
    jne .a57_err
    call handle_mount_drive
    jc .a57_err
    mov ax, word ptr es:[di + 12]
    mov bl, byte ptr es:[di + 14]
    xor bh, bh
    mov si, bx
    lea bx, [sector_buf]
    xor dx, dx
    call fat12_read_lba
    jc .a57_err
    mov ax, si
    mov cl, 5
    shl ax, cl
    lea si, [sector_buf]
    add si, ax
    push cs
    pop ds
    mov cx, [si + 22]
    mov dx, [si + 24]
    call dos_remount_current
    pop ds
    pop es
    pop di
    pop si
    pop bx
    xor ax, ax
    clc
    jmp int21_iret

.a57_set:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds
    mov word ptr cs:[a57_time], cx
    mov word ptr cs:[a57_date], dx
    push cs
    pop es
    call handle_index
    jc .a57_serr
    cmp byte ptr es:[di], H_FILE
    jne .a57_serr
    call handle_mount_drive
    jc .a57_serr
    mov ax, word ptr es:[di + 12]
    mov word ptr cs:[a57_lba], ax
    mov bl, byte ptr es:[di + 14]
    xor bh, bh
    mov si, bx
    lea bx, [sector_buf]
    xor dx, dx
    call fat12_read_lba
    jc .a57_serr
    mov ax, si
    mov cl, 5
    shl ax, cl
    lea si, [sector_buf]
    add si, ax
    push cs
    pop ds
    mov ax, word ptr cs:[a57_time]
    mov [si + 22], ax
    mov ax, word ptr cs:[a57_date]
    mov [si + 24], ax
    mov ax, word ptr cs:[a57_lba]
    lea bx, [sector_buf]
    xor dx, dx
    call fat12_write_lba
    jc .a57_serr
    call dos_remount_current
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

.a57_err:
    call dos_remount_current
    pop ds
    pop es
    pop di
    pop si
    pop bx
    mov ax, 6
    stc
    jmp int21_iret_cf

.a57_serr:
    call dos_remount_current
    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov ax, 6
    stc
    jmp int21_iret_cf

/*
 * AH=5Ah — create unique temp file.
 * DS:DX = ASCIZ directory prefix; name appended in place.
 * CX = attributes. Tail-calls dos_ah3c (same INT frame, no nest).
 */
dos_ah5a:
    push ax
    push bx
    push si
    push di
    mov word ptr cs:[tmp_attrs], cx
    mov word ptr cs:[tmp_name_ctr], 0
    mov si, dx
.a5a_find0:
    cmp byte ptr [si], 0
    je .a5a_got0
    inc si
    jmp .a5a_find0
.a5a_got0:
    mov word ptr cs:[tmp_prefix_end], si
    mov di, si
    mov byte ptr [di], 'R'
    mov byte ptr [di + 1], 'M'
    mov ax, word ptr cs:[tmp_name_ctr]
    mov bx, 2
    mov cx, 4
.a5a_hex:
    push cx
    mov cl, 4
    rol ax, cl
    push ax
    and al, 0x0F
    add al, '0'
    cmp al, '9'
    jbe .a5a_hexd
    add al, 7
.a5a_hexd:
    mov [di + bx], al
    inc bx
    pop ax
    pop cx
    loop .a5a_hex
    mov byte ptr [di + bx], '.'
    inc bx
    mov byte ptr [di + bx], 'T'
    inc bx
    mov byte ptr [di + bx], 'M'
    inc bx
    mov byte ptr [di + bx], 'P'
    inc bx
    mov byte ptr [di + bx], 0
    pop di
    pop si
    pop bx
    pop ax
    mov cx, word ptr cs:[tmp_attrs]
    /* DX still user path buffer with unique name filled */
    jmp dos_ah3c

/*
 * AH=5Bh — create new file (fail if exists).
 * DS:DX = ASCIZ, CX = attrs. AX=handle.
 */
dos_ah5b:
    push bx
    push cx
    push dx
    mov word ptr cs:[tmp_attrs], cx
    push dx
    call path_resolve
    pop dx
    jc .a5b_create
    call path_name83_present
    jc .a5b_create
    mov dx, word ptr cs:[path_resolve_cluster]
    call fat12_find_in_dir
    jc .a5b_create
    pop dx
    pop cx
    pop bx
    mov ax, 0x50
    stc
    jmp int21_iret_cf
.a5b_create:
    pop dx
    pop cx
    pop bx
    mov cx, word ptr cs:[tmp_attrs]
    jmp dos_ah3c

/* AH=5Ch — lock/unlock file region (single-user stub: always succeed) */
dos_ah5c:
    xor ax, ax
    clc
    jmp int21_iret

/*
 * AH=60h — truename: DS:SI source path, ES:DI dest buffer.
 * Writes D:\PATH\FILE form (drive from path or current).
 */
dos_ah60:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push bp

    mov bp, di                   /* dest */
    mov al, [si]
    call .a60_up
    cmp al, 'A'
    jb .a60_nodrv
    cmp al, 'Z'
    ja .a60_nodrv
    cmp byte ptr [si + 1], ':'
    jne .a60_nodrv
    mov bl, al
    sub bl, 'A'
    add si, 2
    jmp .a60_have_drv
.a60_nodrv:
    mov bl, byte ptr cs:[cur_drive]
.a60_have_drv:
    mov al, bl
    add al, 'A'
    mov es:[bp], al
    mov byte ptr es:[bp + 1], ':'
    add bp, 2

    mov al, [si]
    cmp al, '\\'
    je .a60_abs
    cmp al, '/'
    je .a60_abs
    /* relative: emit cwd_path then \\ if needed then SI */
    push si
    push ds
    push cs
    pop ds
    lea si, [cwd_path]
.a60_cwd:
    lodsb
    test al, al
    jz .a60_cwd_done
    mov es:[bp], al
    inc bp
    jmp .a60_cwd
.a60_cwd_done:
    pop ds
    pop si
    cmp byte ptr [si], 0
    je .a60_term
    cmp bp, 2
    je .a60_slash
    cmp byte ptr es:[bp - 1], '\\'
    je .a60_copy
.a60_slash:
    mov byte ptr es:[bp], '\\'
    inc bp
    jmp .a60_copy
.a60_abs:
.a60_copy:
    lodsb
    test al, al
    jz .a60_term
    cmp al, '/'
    jne .a60_store
    mov al, '\\'
.a60_store:
    call .a60_up
    mov es:[bp], al
    inc bp
    jmp .a60_copy
.a60_term:
    mov byte ptr es:[bp], 0
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    xor ax, ax
    clc
    jmp int21_iret

.a60_up:
    cmp al, 'a'
    jb .a60_up_ret
    cmp al, 'z'
    ja .a60_up_ret
    sub al, 0x20
.a60_up_ret:
    ret

init_std_handles:
    push es
    push di
    push cx
    push cs
    pop es
    mov word ptr cs:[max_handles], 20
    lea di, [handles]
    mov cx, MAX_HANDLES_CAP
.ish_clr:
    mov byte ptr es:[di], 0
    add di, HANDLE_SIZE
    loop .ish_clr
    lea di, [handles]
    mov byte ptr es:[di], H_CON
    mov byte ptr es:[di + HANDLE_SIZE], H_CON
    mov byte ptr es:[di + HANDLE_SIZE * 2], H_CON
    mov byte ptr es:[di + HANDLE_SIZE * 3], H_AUX
    mov byte ptr es:[di + HANDLE_SIZE * 4], H_PRN
    pop cx
    pop di
    pop es
    ret

/* Apply FILES= / AH=67: BX = desired count, clamp 5..64. CF if shrink orphans. */
handles_set_max:
    push ax
    push cx
    push si
    push di
    push es
    cmp bx, 5
    jae .hsm_lo
    mov bx, 5
.hsm_lo:
    cmp bx, MAX_HANDLES_CAP
    jbe .hsm_hi
    mov bx, MAX_HANDLES_CAP
.hsm_hi:
    cmp bx, word ptr cs:[max_handles]
    jae .hsm_ok
    push cs
    pop es
    mov cx, word ptr cs:[max_handles]
    mov di, bx
.hsm_chk:
    cmp di, cx
    jae .hsm_ok
    mov ax, di
    shl ax, 1
    shl ax, 1
    shl ax, 1
    shl ax, 1
    lea si, [handles]
    add si, ax
    cmp byte ptr es:[si], 0
    jne .hsm_fail
    inc di
    jmp .hsm_chk
.hsm_fail:
    pop es
    pop di
    pop si
    pop cx
    pop ax
    stc
    ret
.hsm_ok:
    mov word ptr cs:[max_handles], bx
    pop es
    pop di
    pop si
    pop cx
    pop ax
    clc
    ret

handles_apply_files:
    push bx
    mov bx, word ptr cs:[cfg_files]
    call handles_set_max
    pop bx
    ret
