/*
 * Fixed-layout FAT12 I/O for the rmDOS 720 KB boot floppy
 * (matches scripts/fat12.py).
 */

.equ FAT12_SPT, 9
.equ FAT12_HEADS, 2
.equ FAT12_FAT1_LBA, 2
.equ FAT12_FAT2_LBA, 5
.equ FAT12_FAT_SECS, 3
.equ FAT12_ROOT_LBA, 8
.equ FAT12_ROOT_SECS, 7
.equ FAT12_DATA_LBA, 15
.equ FAT12_EOC, 0x0FF8
.equ FAT12_MAX_CLUST, 0x592

/*
 * Read one sector.
 * IN:  AX = LBA, ES:BX = buffer; CS:boot_drive = BIOS drive
 * OUT: CF set on error. AX/BX preserved.
 */
fat12_read_lba:
    push ax
    push bx
    push cx
    push dx
    push di

    mov di, bx
    xor dx, dx
    mov cx, FAT12_SPT
    div cx
    mov cl, dl
    inc cl
    xor dx, dx
    mov bx, FAT12_HEADS
    div bx
    mov ch, al
    mov dh, dl
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
    /* AL=1 ignore → pretend success; else fail */
    cmp al, 1
    je .frl_ok
    stc
    jmp .frl_done
.frl_ok:
    clc
.frl_done:
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * Write one sector.
 * IN:  AX = LBA, ES:BX = buffer; CS:boot_drive = BIOS drive
 * OUT: CF set on error. AX/BX preserved.
 */
fat12_write_lba:
    push ax
    push bx
    push cx
    push dx
    push di

    mov di, bx
    xor dx, dx
    mov cx, FAT12_SPT
    div cx
    mov cl, dl
    inc cl
    xor dx, dx
    mov bx, FAT12_HEADS
    div bx
    mov ch, al
    mov dh, dl
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

    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* AX = cluster → AX = LBA */
fat12_cluster_to_lba:
    dec ax
    dec ax
    add ax, FAT12_DATA_LBA
    ret

/*
 * AX = cluster → AX = next (12-bit). CF on error.
 * Uses CS:fat_buf (must be loaded).
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
 * Load both FAT copies' source (FAT1) into CS:fat_buf (1536 bytes).
 * Uses sector_buf as bounce buffer for INT 13h.
 */
fat12_load_fat:
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
    mov ax, FAT12_FAT1_LBA
    lea di, [fat_buf]
    mov cx, FAT12_FAT_SECS
.flf:
    push cx
    push ax
    lea bx, [sector_buf]
    call fat12_read_lba
    jc .flf_err
    lea si, [sector_buf]
    mov cx, 512
    rep movsb
    pop ax
    pop cx
    inc ax
    loop .flf
    mov byte ptr cs:[fat_dirty], 0
    clc
    jmp .flf_done
.flf_err:
    pop ax
    pop cx
    stc
.flf_done:
    pop ds
    pop es
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    ret

/*
 * Write fat_buf to FAT1 and FAT2 via sector_buf bounce.
 */
/*
 * Write fat_buf to FAT1 and FAT2 via sector_buf bounce.
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
    /* refuse to flush a wiped FAT cache */
    cmp byte ptr cs:[fat_buf], 0xF9
    je .fff_media_ok
    cmp byte ptr cs:[fat_buf], 0xF0
    jb .fff_ok
.fff_media_ok:
    push cs
    pop es
    push cs
    pop ds
    mov ax, FAT12_FAT1_LBA
    lea si, [fat_buf]
    mov cx, FAT12_FAT_SECS
.fff1:
    push cx
    push ax
    push si
    lea di, [sector_buf]
    mov cx, 512
    rep movsb
    lea bx, [sector_buf]
    pop si
    add si, 512
    pop ax
    call fat12_write_lba
    jc .fff_err1
    inc ax
    pop cx
    loop .fff1
    mov ax, FAT12_FAT2_LBA
    lea si, [fat_buf]
    mov cx, FAT12_FAT_SECS
.fff2:
    push cx
    push ax
    push si
    lea di, [sector_buf]
    mov cx, 512
    rep movsb
    lea bx, [sector_buf]
    pop si
    add si, 512
    pop ax
    call fat12_write_lba
    jc .fff_err1
    inc ax
    pop cx
    loop .fff2
    mov byte ptr cs:[fat_dirty], 0
.fff_ok:
    clc
    jmp .fff_done
.fff_err1:
    add sp, 2
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
 * AX = cluster → AX = 12-bit FAT value from fat_buf (no disk I/O).
 */
fat12_get_fat:
    push bx
    push cx
    push si
    mov bx, ax
    push ax
    shr bx, 1
    add bx, ax
    lea si, [fat_buf]
    add si, bx
    mov ax, cs:[si]
    pop bx
    test bx, 1
    jnz .gf_odd
    and ax, 0x0FFF
    jmp .gf_ok
.gf_odd:
    mov cl, 4
    shr ax, cl
    and ax, 0x0FFF
.gf_ok:
    pop si
    pop cx
    pop bx
    ret

/*
 * AX = cluster, DX = value (12-bit). Updates fat_buf + dirty.
 */
fat12_set_fat:
    push ax
    push bx
    push cx
    push si
    mov bx, ax
    push ax
    shr bx, 1
    add bx, ax
    lea si, [fat_buf]
    add si, bx
    mov ax, cs:[si]
    pop bx
    test bx, 1
    jnz .sf_odd
    and ax, 0xF000
    and dx, 0x0FFF
    or ax, dx
    jmp .sf_store
.sf_odd:
    and ax, 0x000F
    and dx, 0x0FFF
    mov cl, 4
    shl dx, cl
    or ax, dx
.sf_store:
    mov cs:[si], ax
    mov byte ptr cs:[fat_dirty], 1
    pop si
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
    cmp bx, FAT12_MAX_CLUST
    jae .fac_fail
    mov ax, bx
    call fat12_get_fat
    test ax, ax
    jz .fac_got
    inc bx
    jmp .fac
.fac_got:
    mov ax, bx
    mov dx, 0x0FFF
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
    cmp ax, FAT12_EOC
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
    mov bp, FAT12_ROOT_LBA
    mov dx, FAT12_ROOT_SECS
.ffd_sec:
    test dx, dx
    jz .ffd_fail
    lea bx, [sector_buf]
    mov ax, bp
    call fat12_read_lba
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
    mov bp, ax                   /* cluster */
    push cs
    pop es
    mov ax, word ptr cs:[last_dir_lba]
    lea bx, [sector_buf]
    call fat12_read_lba
    jc .fud_err
    mov bx, word ptr cs:[last_dir_idx]
    mov cl, 5
    shl bx, cl
    lea di, [sector_buf]
    add di, bx
    mov es:[di + 26], bp
    mov es:[di + 28], dx
    mov ax, word ptr cs:[last_size_hi]
    mov word ptr es:[di + 30], ax
    mov ax, word ptr cs:[last_dir_lba]
    lea bx, [sector_buf]
    call fat12_write_lba
    jc .fud_err
    clc
    jmp .fud_done
.fud_err:
    stc
.fud_done:
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
    cmp bx, FAT12_ROOT_SECS
    jae .fds_err
    mov ax, FAT12_ROOT_LBA
    add ax, bx
    clc
    jmp .fds_done
.fds_sub:
    mov ax, dx
    mov cx, bx
.fds_walk:
    jcxz .fds_got
    call fat12_next_cluster
    cmp ax, FAT12_EOC
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

    push ax
    lea bx, [sector_buf]
    call fat12_read_lba
    pop bx                       /* BX = LBA */
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
    cmp ax, FAT12_EOC
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
    push ax
    lea di, [sector_buf]
    xor ax, ax
    mov cx, 256
    rep stosw
    pop ax
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
    call fat12_write_lba
    pop ds
    pop es
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    ret
