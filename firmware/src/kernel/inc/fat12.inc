/*
 * BPB-driven FAT12/FAT16 I/O for the current volume (CS:boot_drive).
 * Call fat12_init_bpb once at mount before fat12_load_fat.
 */

.equ FAT12_EOC, 0x0FF8
.equ FAT16_EOC, 0xFFF8
.equ FAT_WIN_SECS, 2             /* 1KB window (covers FAT12 span) */

/*
 * Read the VBR and fill CS:bpb_* and derived absolute LBAs.
 *
 * A valid MBR with a primary DOS partition (01h/04h/06h) is mounted through
 * its first such partition.  Otherwise sector zero remains a whole-disk VBR.
 * OUT: CF set on failure.
 */
fat12_init_bpb:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds

    push cs
    pop es
    push cs
    pop ds
    lea bx, [sector_buf]
    mov ax, 0x0201
    mov cx, 0x0001
    xor dh, dh
    mov dl, cs:[boot_drive]
    int 0x13
    jc .fib_fail

    mov word ptr [vol_base_lba], 0
    cmp word ptr [sector_buf + 510], 0xAA55
    jne .fib_vbr
    mov si, 0x1BE
    mov cx, 4
.fib_part:
    mov al, [sector_buf + si + 4]
    cmp al, 0x01
    je .fib_dos_part
    cmp al, 0x04
    je .fib_dos_part
    cmp al, 0x06
    jne .fib_next_part
.fib_dos_part:
    cmp word ptr [sector_buf + si + 10], 0
    jne .fib_fail                /* rmDOS supports era-sized LBAs only */
    mov ax, [sector_buf + si + 8]
    test ax, ax
    jz .fib_next_part
    mov [vol_base_lba], ax
    /* Read the VBR at the partition's recorded CHS start. */
    mov bx, 0x0000
    mov dh, [sector_buf + si + 1]
    mov cl, [sector_buf + si + 2]
    mov ch, [sector_buf + si + 3]
    mov dl, cs:[boot_drive]
    mov ax, 0x0201
    int 0x13
    jc .fib_fail
    jmp .fib_vbr
.fib_next_part:
    add si, 16
    loop .fib_part
.fib_vbr:
    /* Require 512 BPS */
    cmp word ptr [sector_buf + 11], 512
    jne .fib_fail

    mov al, [sector_buf + 13]
    mov [bpb_spc], al
    mov ax, [sector_buf + 14]
    mov [bpb_reserved], ax
    mov al, [sector_buf + 16]
    mov [bpb_fats], al
    mov ax, [sector_buf + 17]
    mov [bpb_root_ents], ax
    mov ax, [sector_buf + 19]
    mov [bpb_totsec], ax
    mov word ptr [bpb_totsec_hi], 0
    test ax, ax
    jnz .fib_tot_ok
    mov ax, [sector_buf + 32]
    mov [bpb_totsec], ax
    mov ax, [sector_buf + 34]
    mov [bpb_totsec_hi], ax
.fib_tot_ok:
    mov al, [sector_buf + 21]
    mov [bpb_media], al
    mov ax, [sector_buf + 22]
    mov [bpb_spf], ax
    mov ax, [sector_buf + 24]
    mov [bpb_spt], ax
    mov ax, [sector_buf + 26]
    mov [bpb_heads], ax

    /* fat1_lba = reserved */
    mov ax, [bpb_reserved]
    mov [bpb_fat1_lba], ax
    /* fat2_lba = reserved + spf */
    mov bx, [bpb_spf]
    add ax, bx
    mov [bpb_fat2_lba], ax
    /* root_lba = reserved + fats*spf */
    mov ax, [bpb_reserved]
    mov cl, [bpb_fats]
    xor ch, ch
.fib_fats:
    add ax, bx
    loop .fib_fats
    mov [bpb_root_lba], ax
    /* root_secs = root_ents / 16 */
    mov ax, [bpb_root_ents]
    mov cl, 4
    shr ax, cl
    mov [bpb_root_secs], ax
    /* data_lba = root_lba + root_secs */
    mov bx, [bpb_root_lba]
    add bx, ax
    mov [bpb_data_lba], bx
    /* max_clust = 2 + (totsec - data_lba) / spc  (totsec may be 32-bit) */
    mov ax, [bpb_totsec]
    mov dx, [bpb_totsec_hi]
    sub ax, bx
    sbb dx, 0
    mov bl, [bpb_spc]
    xor bh, bh
    div bx                       /* assume quot fits in AX for era sizes */
    add ax, 2
    mov [bpb_max_clust], ax

    /* The BPB addresses are volume-relative; all filesystem I/O is physical. */
    mov ax, [vol_base_lba]
    add [bpb_fat1_lba], ax
    add [bpb_fat2_lba], ax
    add [bpb_root_lba], ax
    add [bpb_data_lba], ax

    /* FAT12 if clusters <= 4085, else FAT16 */
    mov byte ptr [bpb_fat_type], 12
    mov word ptr [fat_eoc], FAT12_EOC
    mov bx, [bpb_max_clust]
    cmp bx, 4085
    jbe .fib_type_ok
    mov byte ptr [bpb_fat_type], 16
    mov word ptr [fat_eoc], FAT16_EOC
.fib_type_ok:
    mov word ptr [fat_win_sec], 0xFFFF
    mov byte ptr [fat_dirty], 0

    clc
    jmp .fib_done
.fib_fail:
    stc
.fib_done:
    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * Read one sector.
 * IN:  DX:AX = LBA (DX high), ES:BX = buffer; CS:boot_drive = BIOS drive
 * OUT: CF set on error. DX:AX/BX preserved.
 */
fat12_read_lba:
    push ax
    push bx
    push cx
    push dx
    push di
    push si

    mov di, bx
    mov cx, cs:[bpb_spt]
    div cx                       /* DX:AX / spt → AX=temp, DX=sec rem */
    mov si, dx
    inc si                       /* 1-based sector */
    xor dx, dx
    mov bx, cs:[bpb_heads]
    div bx                       /* AX=cyl, DX=head */
    mov dh, dl
    mov ch, al
    mov cl, 6
    shl ah, cl
    mov bx, si
    or ah, bl
    mov cl, ah
    mov dl, cs:[boot_drive]
    mov bx, di
    mov ah, 0x02
    mov al, 0x01
    int 0x13
    jnc .frl_ok
    push bx
    xor bh, bh                   /* read */
    call invoke_int24_disk
    pop bx
    cmp al, 1
    je .frl_ok
    stc
    jmp .frl_done
.frl_ok:
    clc
.frl_done:
    pop si
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * Write one sector.
 * IN:  DX:AX = LBA (DX high), ES:BX = buffer; CS:boot_drive = BIOS drive
 * OUT: CF set on error. DX:AX/BX preserved.
 */
fat12_write_lba:
    push ax
    push bx
    push cx
    push dx
    push di
    push si

    mov di, bx
    mov cx, cs:[bpb_spt]
    div cx
    mov si, dx
    inc si
    xor dx, dx
    mov bx, cs:[bpb_heads]
    div bx
    mov dh, dl
    mov ch, al
    mov cl, 6
    shl ah, cl
    mov bx, si
    or ah, bl
    mov cl, ah
    mov dl, cs:[boot_drive]
    mov bx, di
    mov ah, 0x03
    mov al, 0x01
    int 0x13
    jnc .fwl_ok
    push bx
    mov bh, 1                    /* write */
    call invoke_int24_disk
    pop bx
    cmp al, 1
    je .fwl_ok
    stc
    jmp .fwl_done
.fwl_ok:
    clc
.fwl_done:
    pop si
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* AX = cluster → DX:AX = LBA */
fat12_cluster_to_lba:
    push bx
    dec ax
    dec ax
    mov bl, cs:[bpb_spc]
    xor bh, bh
    mul bx                       /* DX:AX = (clust-2)*spc */
    add ax, cs:[bpb_data_lba]
    adc dx, 0
    pop bx
    ret

/*
 * AX = cluster → AX = next. CF on error.
 * Uses windowed FAT cache.
 */
fat12_next_cluster:
    push bx
    call fat12_get_fat
    clc
    pop bx
    ret

/*
 * Convert ASCIZ at DS:SI to 11-byte 8.3 in CS:name83.
 */
fat12_path_to_83:
    push ax
    push bx
    push cx
    push di
    push es
    push si

    push cs
    pop es
    lea di, [name83]
    mov cx, 11
    mov al, ' '
    rep stosb
    lea di, [name83]

    mov cx, 8
.p83_stem:
    mov al, [si]
    test al, al
    jz .p83_ok
    cmp al, '.'
    je .p83_dot
    inc si
    cmp al, 'a'
    jb .p83_up
    cmp al, 'z'
    ja .p83_up
    sub al, 0x20
.p83_up:
    stosb
    loop .p83_stem
    mov al, [si]
    test al, al
    jz .p83_ok
    cmp al, '.'
    jne .p83_bad
.p83_dot:
    inc si
    lea di, [name83 + 8]
    mov cx, 3
.p83_ext:
    mov al, [si]
    test al, al
    jz .p83_ok
    inc si
    cmp al, 'a'
    jb .p83_ext_up
    cmp al, 'z'
    ja .p83_ext_up
    sub al, 0x20
.p83_ext_up:
    stosb
    loop .p83_ext
.p83_ok:
    clc
    jmp .p83_done
.p83_bad:
    stc
.p83_done:
    pop si
    pop es
    pop di
    pop cx
    pop bx
    pop ax
    ret

/*
 * Find CS:name83 in root.
 * OUT: CF clear → AX=start cluster, CX=size (lo word)
 * Stores size hi in CS:last_size_hi.
 */
fat12_find_root:
    push bx
    push dx
    xor dx, dx
    call fat12_find_in_dir
    pushf
    mov word ptr cs:[last_size_hi], bx
    popf
    pop dx
    pop bx
    ret

/*
 * Invalidate FAT window (call after mount). Window loads on demand.
 */
fat12_load_fat:
    push ax
    call fat12_flush_fat
    mov word ptr cs:[fat_win_sec], 0xFFFF
    mov byte ptr cs:[fat_dirty], 0
    clc
    pop ax
    ret

/*
 * Flush dirty FAT window to FAT1 and FAT2.
 */
fat12_flush_fat:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds
    cmp byte ptr cs:[fat_dirty], 0
    je .fff_ok
    cmp word ptr cs:[fat_win_sec], 0xFFFF
    je .fff_ok
    push cs
    pop es
    push cs
    pop ds
    xor si, si                   /* offset in fat_buf */
    xor cx, cx                   /* window index */
.fff_sec:
    cmp cx, FAT_WIN_SECS
    jae .fff_fin
    mov ax, cs:[fat_win_sec]
    add ax, cx
    cmp ax, cs:[bpb_spf]
    jae .fff_fin
    lea di, [sector_buf]
    lea ax, [fat_buf]
    add ax, si
    push si
    push cx
    mov si, ax
    mov cx, 256
    rep movsw
    pop cx
    pop si
    /* FAT1 */
    mov ax, cs:[bpb_fat1_lba]
    add ax, cs:[fat_win_sec]
    add ax, cx
    xor dx, dx
    lea bx, [sector_buf]
    push cx
    call fat12_write_lba
    pop cx
    jc .fff_err
    /* FAT2 */
    mov ax, cs:[bpb_fat2_lba]
    add ax, cs:[fat_win_sec]
    add ax, cx
    xor dx, dx
    lea bx, [sector_buf]
    push cx
    call fat12_write_lba
    pop cx
    jc .fff_err
    add si, 512
    inc cx
    jmp .fff_sec
.fff_fin:
    mov byte ptr cs:[fat_dirty], 0
.fff_ok:
    clc
    jmp .fff_done
.fff_err:
    stc
.fff_done:
    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * Ensure fat_buf covers FAT sectors starting at BX (2 sectors).
 * CF on I/O error.
 */
fat_ensure_window:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds
    mov ax, bx                   /* desired start */
    mov dx, cs:[bpb_spf]
    cmp dx, 2
    jae .few_clamp
    xor ax, ax
    jmp .few_have_start
.few_clamp:
    mov cx, dx
    dec cx
    dec cx                       /* spf-2 */
    cmp ax, cx
    jbe .few_have_start
    mov ax, cx
.few_have_start:
    cmp ax, cs:[fat_win_sec]
    je .few_ok
    call fat12_flush_fat
    jc .few_fail
    mov cs:[fat_win_sec], ax
    push cs
    pop es
    push cs
    pop ds
    lea di, [fat_buf]
    mov cx, FAT_WIN_SECS
    mov dx, cs:[bpb_spf]
    sub dx, ax
    cmp cx, dx
    jbe .few_rd
    mov cx, dx
.few_rd:
    push cx
    push ax
    mov bx, ax
    mov ax, cs:[bpb_fat1_lba]
    add ax, bx
    xor dx, dx
    lea bx, [sector_buf]
    call fat12_read_lba
    jc .few_rd_err
    lea si, [sector_buf]
    mov cx, 512
    rep movsb
    pop ax
    inc ax
    pop cx
    loop .few_rd
    mov byte ptr cs:[fat_dirty], 0
.few_ok:
    clc
    jmp .few_done
.few_rd_err:
    pop ax
    pop cx
.few_fail:
    mov word ptr cs:[fat_win_sec], 0xFFFF
    stc
.few_done:
    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * AX = cluster → AX = FAT value (12 or 16-bit).
 */
fat12_get_fat:
    push bx
    push cx
    push dx
    push si
    cmp byte ptr cs:[bpb_fat_type], 16
    je .gf16
    /* FAT12: offset = clust + clust/2 */
    mov bx, ax
    push ax
    shr bx, 1
    add bx, ax                   /* byte offset */
    mov ax, bx
    mov cl, 9
    shr ax, cl                   /* sector index */
    push bx
    mov bx, ax
    call fat_ensure_window
    pop bx
    jc .gf_err
    /* offset within window */
    mov ax, cs:[fat_win_sec]
    mov cl, 9
    shl ax, cl
    sub bx, ax
    lea si, [fat_buf]
    add si, bx
    mov ax, cs:[si]
    pop bx                       /* cluster */
    test bx, 1
    jnz .gf_odd
    and ax, 0x0FFF
    jmp .gf_ok
.gf_odd:
    mov cl, 4
    shr ax, cl
    and ax, 0x0FFF
    jmp .gf_ok
.gf16:
    mov bx, ax
    shl bx, 1                    /* byte offset */
    mov ax, bx
    mov cl, 9
    shr ax, cl
    push bx
    mov bx, ax
    call fat_ensure_window
    pop bx
    jc .gf_err16
    mov ax, cs:[fat_win_sec]
    mov cl, 9
    shl ax, cl
    sub bx, ax
    lea si, [fat_buf]
    add si, bx
    mov ax, cs:[si]
    jmp .gf_ok
.gf_err:
    pop bx
.gf_err16:
    xor ax, ax
.gf_ok:
    pop si
    pop dx
    pop cx
    pop bx
    ret

/*
 * AX = cluster, DX = value. Updates window + dirty.
 */
fat12_set_fat:
    push ax
    push bx
    push cx
    push dx
    push si
    mov cx, dx                   /* value */
    cmp byte ptr cs:[bpb_fat_type], 16
    je .sf16
    mov bx, ax
    push ax
    shr bx, 1
    add bx, ax
    mov ax, bx
    push cx
    mov cl, 9
    shr ax, cl
    push bx
    mov bx, ax
    call fat_ensure_window
    pop bx
    pop cx
    jc .sf_fail
    mov ax, cs:[fat_win_sec]
    push cx
    mov cl, 9
    shl ax, cl
    pop cx
    sub bx, ax
    lea si, [fat_buf]
    add si, bx
    mov ax, cs:[si]
    pop bx                       /* cluster */
    test bx, 1
    jnz .sf_odd
    and ax, 0xF000
    and cx, 0x0FFF
    or ax, cx
    jmp .sf_store
.sf_odd:
    and ax, 0x000F
    and cx, 0x0FFF
    push cx
    mov cl, 4
    pop dx
    shl dx, cl
    or ax, dx
    jmp .sf_store
.sf16:
    mov bx, ax
    shl bx, 1
    mov ax, bx
    push cx
    mov cl, 9
    shr ax, cl
    push bx
    mov bx, ax
    call fat_ensure_window
    pop bx
    pop cx
    jc .sf_fail16
    mov ax, cs:[fat_win_sec]
    push cx
    mov cl, 9
    shl ax, cl
    pop cx
    sub bx, ax
    lea si, [fat_buf]
    add si, bx
    mov ax, cx
.sf_store:
    mov cs:[si], ax
    mov byte ptr cs:[fat_dirty], 1
    clc
    jmp .sf_done
.sf_fail:
    pop bx
.sf_fail16:
    stc
.sf_done:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * Allocate a free cluster. OUT: AX=cluster, CF on full/error.
 * Requires fat loaded. Sets entry to EOC.
 */
fat12_alloc_cluster:
    push bx
    push cx
    push dx
    mov bx, 2
.fac:
    cmp bx, cs:[bpb_max_clust]
    jae .fac_fail
    mov ax, bx
    call fat12_get_fat
    test ax, ax
    jz .fac_got
    inc bx
    jmp .fac
.fac_got:
    mov ax, bx
    mov dx, cs:[fat_eoc]
    call fat12_set_fat
    mov ax, bx
    clc
    jmp .fac_done
.fac_fail:
    stc
.fac_done:
    pop dx
    pop cx
    pop bx
    ret

/*
 * Free chain starting at AX (0 = nop). Requires fat loaded.
 */
fat12_free_chain:
    push ax
    push bx
    push dx
    test ax, ax
    jz .ffc_ok
.ffc:
    cmp ax, 2
    jb .ffc_ok
    cmp ax, cs:[fat_eoc]
    jae .ffc_ok
    mov bx, ax
    call fat12_get_fat
    test ax, ax
    jz .ffc_zero
    mov dx, ax
    mov ax, bx
    push dx
    xor dx, dx
    call fat12_set_fat
    pop ax
    jmp .ffc
.ffc_zero:
    mov ax, bx
    xor dx, dx
    call fat12_set_fat
.ffc_ok:
    clc
    pop dx
    pop bx
    pop ax
    ret

/*
 * Find free root dirent. OUT: last_dir_lba/idx set, CF if full.
 * Leaves sector in sector_buf.
 */
fat12_find_free_dirent:
    push ax
    push bx
    push cx
    push dx
    push si
    push es
    push bp
    push di
    push cs
    pop es
    mov bp, cs:[bpb_root_lba]
    mov dx, cs:[bpb_root_secs]
.ffd_sec:
    test dx, dx
    jz .ffd_fail
    lea bx, [sector_buf]
    mov ax, bp
    push dx
    xor dx, dx
    call fat12_read_lba
    pop dx
    jc .ffd_fail
    xor si, si
.ffd_ent:
    cmp si, 16
    jae .ffd_next
    mov bx, si
    mov cl, 5
    shl bx, cl
    lea di, [sector_buf]
    add di, bx
    mov al, es:[di]
    test al, al
    jz .ffd_got
    cmp al, 0xE5
    je .ffd_got
    inc si
    jmp .ffd_ent
.ffd_next:
    inc bp
    dec dx
    jmp .ffd_sec
.ffd_got:
    mov word ptr cs:[last_dir_lba], bp
    mov word ptr cs:[last_dir_idx], si
    clc
    jmp .ffd_done
.ffd_fail:
    stc
.ffd_done:
    pop di
    pop bp
    pop es
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * Write 11-byte CS:name83 into slot at last_dir_*; cluster=0 size=0 attr=20h.
 * Sector must already be in sector_buf.
 */
fat12_create_dirent:
    push ax
    push bx
    push cx
    push si
    push di
    push es
    push ds
    push cs
    pop es
    push cs
    pop ds
    mov bx, word ptr [last_dir_idx]
    mov cl, 5
    shl bx, cl
    lea di, [sector_buf]
    add di, bx
    push di
    lea si, [name83]
    mov cx, 11
    rep movsb
    pop di
    mov byte ptr es:[di + 11], 0x20
    mov word ptr es:[di + 12], 0
    mov word ptr es:[di + 14], 0
    mov word ptr es:[di + 16], 0
    mov word ptr es:[di + 18], 0
    mov word ptr es:[di + 20], 0
    mov word ptr es:[di + 22], 0
    mov word ptr es:[di + 24], 0
    mov word ptr es:[di + 26], 0
    mov word ptr es:[di + 28], 0
    mov word ptr es:[di + 30], 0
    mov ax, word ptr [last_dir_lba]
    lea bx, [sector_buf]
    xor dx, dx
    call fat12_write_lba
    pop ds
    pop es
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    ret

/*
 * Update dirent size and start cluster (AX) at last_dir_*.
 * DX = size lo; CS:last_size_hi = size hi (caller sets; default 0).
 * Call sites previously assuming +30 written as 0: set last_size_hi=0
 * before calling (or leave zeroed at init).
 */
fat12_update_dirent:
    push ax
    push bx
    push cx
    push dx
    push di
    push es
    push bp
    push si
    mov bp, ax                   /* cluster */
    mov si, dx                   /* size lo */
    push cs
    pop es
    mov ax, word ptr cs:[last_dir_lba]
    lea bx, [sector_buf]
    xor dx, dx
    call fat12_read_lba
    jc .fud_err
    mov bx, word ptr cs:[last_dir_idx]
    mov cl, 5
    shl bx, cl
    lea di, [sector_buf]
    add di, bx
    mov es:[di + 26], bp
    mov es:[di + 28], si
    mov ax, word ptr cs:[last_size_hi]
    mov word ptr es:[di + 30], ax
    mov ax, word ptr cs:[last_dir_lba]
    lea bx, [sector_buf]
    xor dx, dx
    call fat12_write_lba
    jc .fud_err
    clc
    jmp .fud_done
.fud_err:
    stc
.fud_done:
    pop si
    pop bp
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * Mark dirent at last_dir_* deleted (0xE5).
 */
fat12_delete_dirent:
    push ax
    push bx
    push cx
    push di
    push es
    push cs
    pop es
    mov ax, word ptr cs:[last_dir_lba]
    lea bx, [sector_buf]
    xor dx, dx
    call fat12_read_lba
    jc .fdd_err
    mov bx, word ptr cs:[last_dir_idx]
    mov cl, 5
    shl bx, cl
    lea di, [sector_buf]
    add di, bx
    mov byte ptr es:[di], 0xE5
    mov ax, word ptr cs:[last_dir_lba]
    lea bx, [sector_buf]
    xor dx, dx
    call fat12_write_lba
    jc .fdd_err
    clc
    jmp .fdd_done
.fdd_err:
    stc
.fdd_done:
    pop es
    pop di
    pop cx
    pop bx
    pop ax
    ret

/*
 * Map directory sector index to LBA.
 * IN:  DX = dir_cluster (0 = root), BX = sector_index within dir
 * OUT: AX = LBA; CF set if past end of directory
 */
fat12_dir_sector:
    push bx
    push cx
    push dx
    test dx, dx
    jnz .fds_sub
    cmp bx, cs:[bpb_root_secs]
    jae .fds_err
    mov ax, cs:[bpb_root_lba]
    add ax, bx
    clc
    jmp .fds_done
.fds_sub:
    mov ax, dx
    mov cx, bx
.fds_walk:
    jcxz .fds_got
    call fat12_next_cluster
    cmp ax, cs:[fat_eoc]
    jae .fds_err
    cmp ax, 2
    jb .fds_err
    dec cx
    jmp .fds_walk
.fds_got:
    call fat12_cluster_to_lba
    clc
    jmp .fds_done
.fds_err:
    stc
.fds_done:
    pop dx
    pop cx
    pop bx
    ret

/*
 * Find CS:name83 in directory DX (0 = root).
 * OUT: CF clear → AX=start cluster, CX=size lo, BX=size hi (dirent+30)
 * Sets last_dir_lba / last_dir_idx. Skips volume labels (attr bit 0x08).
 * Directories still return their start cluster.
 */
fat12_find_in_dir:
    push dx
    push si
    push di
    push es
    push bp

    push cs
    pop es
    mov bp, dx                   /* dir cluster */
    xor dx, dx                   /* sector index */

.fid_sec:
    mov bx, dx
    push dx
    mov dx, bp
    call fat12_dir_sector
    pop dx
    jc .fid_nf

    push dx                      /* sector index */
    xor dx, dx                   /* LBA high */
    lea bx, [sector_buf]
    call fat12_read_lba
    pop dx                       /* restore sector index */
    mov bx, ax                   /* LBA (read preserves AX) */
    jc .fid_nf

    xor si, si
.fid_ent:
    cmp si, 16
    jae .fid_next
    push bx
    mov bx, si
    mov cl, 5
    shl bx, cl
    lea di, [sector_buf]
    add di, bx
    pop bx
    mov al, es:[di]
    test al, al
    jz .fid_nf
    cmp al, 0xE5
    je .fid_skip
    mov al, es:[di + 11]
    test al, 0x08
    jnz .fid_skip

    push ds
    push si
    push di
    push cs
    pop ds
    lea si, [name83]
    mov cx, 11
    repe cmpsb
    pop di
    pop si
    pop ds
    jne .fid_skip

    mov ax, es:[di + 26]
    mov cx, es:[di + 28]
    push word ptr es:[di + 30]
    mov word ptr cs:[last_dir_lba], bx
    mov word ptr cs:[last_dir_idx], si
    pop bx                       /* size hi */
    clc
    jmp .fid_done

.fid_skip:
    inc si
    jmp .fid_ent

.fid_next:
    inc dx
    jmp .fid_sec

.fid_nf:
    stc
.fid_done:
    pop bp
    pop es
    pop di
    pop si
    pop dx
    ret

/*
 * Find free/deleted dirent in directory DX (0 = root).
 * Sets last_dir_*; leaves sector in sector_buf.
 * Subdirs: if full, allocate + zero a new cluster, link to chain, use entry 0.
 * CF set on failure.
 */
fat12_find_free_in_dir:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push bp

    push cs
    pop es
    mov bp, dx                   /* dir cluster */
    xor dx, dx                   /* sector index */

.ffid_sec:
    mov bx, dx
    push dx
    mov dx, bp
    call fat12_dir_sector
    pop dx
    jc .ffid_extend

    push ax
    lea bx, [sector_buf]
    xor dx, dx
    call fat12_read_lba
    pop bx                       /* LBA */
    jc .ffid_fail

    xor si, si
.ffid_ent:
    cmp si, 16
    jae .ffid_next
    push bx
    mov bx, si
    mov cl, 5
    shl bx, cl
    lea di, [sector_buf]
    add di, bx
    pop bx
    mov al, es:[di]
    test al, al
    jz .ffid_got
    cmp al, 0xE5
    je .ffid_got
    inc si
    jmp .ffid_ent

.ffid_next:
    inc dx
    jmp .ffid_sec

.ffid_got:
    mov word ptr cs:[last_dir_lba], bx
    mov word ptr cs:[last_dir_idx], si
    clc
    jmp .ffid_done

.ffid_extend:
    /* root cannot grow */
    test bp, bp
    jz .ffid_fail
    /* find last cluster in chain */
    mov ax, bp
.ffid_last:
    mov bx, ax
    call fat12_next_cluster
    cmp ax, cs:[fat_eoc]
    jae .ffid_got_last
    cmp ax, 2
    jb .ffid_fail
    jmp .ffid_last
.ffid_got_last:
    /* BX = last cluster; allocate and link */
    call fat12_alloc_cluster
    jc .ffid_fail
    mov dx, ax                   /* new cluster as FAT value */
    push ax
    mov ax, bx
    call fat12_set_fat
    pop ax                       /* AX = new cluster */
    push ax
    call fat12_cluster_to_lba
    mov word ptr cs:[last_dir_lba], ax
    mov word ptr cs:[last_dir_idx], 0
    /* zero sector_buf and write */
    push dx
    push ax
    lea di, [sector_buf]
    xor ax, ax
    mov cx, 256
    rep stosw
    pop ax
    pop dx
    lea bx, [sector_buf]
    call fat12_write_lba
    pop ax                       /* new cluster (discard) */
    jc .ffid_fail
    clc
    jmp .ffid_done

.ffid_fail:
    stc
.ffid_done:
    pop bp
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * Write CS:name83 into slot at last_dir_*; cluster=0 size=0; AL=attribute.
 * Sector must already be in sector_buf (from find_free).
 */
fat12_create_dirent_attr:
    push ax
    push bx
    push cx
    push si
    push di
    push es
    push ds
    push cs
    pop es
    push cs
    pop ds
    mov ah, al                   /* save attribute */
    mov bx, word ptr [last_dir_idx]
    mov cl, 5
    shl bx, cl
    lea di, [sector_buf]
    add di, bx
    push di
    lea si, [name83]
    mov cx, 11
    rep movsb
    pop di
    mov byte ptr es:[di + 11], ah
    mov word ptr es:[di + 12], 0
    mov word ptr es:[di + 14], 0
    mov word ptr es:[di + 16], 0
    mov word ptr es:[di + 18], 0
    mov word ptr es:[di + 20], 0
    mov word ptr es:[di + 22], 0
    mov word ptr es:[di + 24], 0
    mov word ptr es:[di + 26], 0
    mov word ptr es:[di + 28], 0
    mov word ptr es:[di + 30], 0
    mov ax, word ptr [last_dir_lba]
    lea bx, [sector_buf]
    xor dx, dx
    call fat12_write_lba
    pop ds
    pop es
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    ret
