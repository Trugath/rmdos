/*
 * Logical drive map: letter → (BIOS DL, partition base LBA).
 * A:/B: are floppies; hard-disk DOS primaries (01/04/06) get C: onward
 * in slot order per disk, then logicals inside extended (05h/0Fh) EBR
 * chains; whole-disk FAT VBR gets one letter at LBA 0.
 * Base LBAs are 16-bit (hi word of partition LBA must be 0).
 */

.equ DRIVEMAP_MAX, 8

/*
 * Rebuild CS:drive_map / num_drives. Uses CS:sector_buf.
 * Scratch: drm_ext_base, drm_ebr_lba, drm_spt, drm_heads, drm_bios_dl.
 */
dos_rebuild_drivemap:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds
    push cs
    pop ds
    push cs
    pop es

    mov byte ptr [drive_map_bios], 0
    mov word ptr [drive_map_base], 0
    mov byte ptr [drive_map_bios + 1], 1
    mov word ptr [drive_map_base + 2], 0
    mov byte ptr [num_drives], 2

    mov dl, 0x80
.drm_disk:
    mov byte ptr [drm_bios_dl], dl
    push dx
    mov ah, 0x08
    int 0x13
    pop dx
    jc .drm_done

    mov al, cl
    and ax, 0x003F
    jz .drm_next_disk
    mov word ptr [drm_spt], ax
    mov al, dh
    xor ah, ah
    inc ax
    mov word ptr [drm_heads], ax

    push dx
    lea bx, [sector_buf]
    mov ax, 0x0201
    mov cx, 0x0001
    xor dh, dh
    int 0x13
    pop dx
    jc .drm_next_disk

    cmp word ptr [sector_buf + 510], 0xAA55
    jne .drm_try_vbr

    mov byte ptr [drm_saw_mbr], 1

    /* Add DOS primaries first */
    mov si, 0x1BE
    mov cx, 4
.drm_add_pri:
    mov al, [sector_buf + si + 4]
    call drm_is_dos_type
    jnc .drm_add_pri_next
    call drm_entry_lba16_ok
    jnc .drm_add_pri_next
    mov ax, [sector_buf + si + 8]
    call drm_push
    jc .drm_done
.drm_add_pri_next:
    add si, 16
    loop .drm_add_pri

    /* Collect extended bases (max 4), then walk — walk overwrites sector_buf */
    mov word ptr [drm_ext_list], 0
    mov word ptr [drm_ext_list + 2], 0
    mov word ptr [drm_ext_list + 4], 0
    mov word ptr [drm_ext_list + 6], 0
    xor di, di
    mov si, 0x1BE
    mov cx, 4
.drm_col_ext:
    mov al, [sector_buf + si + 4]
    call drm_is_ext_type
    jnc .drm_col_next
    call drm_entry_lba16_ok
    jnc .drm_col_next
    mov ax, [sector_buf + si + 8]
    mov [drm_ext_list + di], ax
    add di, 2
.drm_col_next:
    add si, 16
    loop .drm_col_ext

    xor di, di
    mov cx, 4
.drm_walk_list:
    mov ax, [drm_ext_list + di]
    test ax, ax
    jz .drm_walk_next
    mov word ptr [drm_ext_base], ax
    call drm_walk_extended
    jc .drm_done
.drm_walk_next:
    add di, 2
    loop .drm_walk_list

    cmp byte ptr [num_drives], 2
    jne .drm_next_disk
    cmp byte ptr [drm_saw_mbr], 0
    jne .drm_next_disk
    jmp .drm_try_vbr

.drm_try_vbr:
    mov byte ptr [drm_saw_mbr], 0
    cmp word ptr [sector_buf + 11], 512
    jne .drm_next_disk
    cmp byte ptr [num_drives], 2
    jne .drm_next_disk
    xor ax, ax
    call drm_push
    jc .drm_done

.drm_next_disk:
    mov byte ptr [drm_saw_mbr], 0
    mov dl, byte ptr [drm_bios_dl]
    inc dl
    cmp dl, 0x84
    jb .drm_disk

.drm_done:
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
 * Walk EBR chain starting at drm_ext_base. DS=CS. CF if map full.
 */
drm_walk_extended:
    push ax
    push bx
    push cx
    push dx
    push si
    mov ax, word ptr [drm_ext_base]
    mov word ptr [drm_ebr_lba], ax
.drm_ebr_loop:
    mov ax, word ptr [drm_ebr_lba]
    lea bx, [sector_buf]
    call drm_read_lba
    jc .drm_ebr_done_clc

    cmp word ptr [sector_buf + 510], 0xAA55
    jne .drm_ebr_done_clc

    mov si, 0x1BE
    mov al, [sector_buf + si + 4]
    call drm_is_dos_type
    jnc .drm_ebr_link
    call drm_entry_lba16_ok
    jnc .drm_ebr_link
    mov ax, word ptr [drm_ebr_lba]
    add ax, [sector_buf + si + 8]
    jc .drm_ebr_link
    call drm_push
    jc .drm_ebr_full

.drm_ebr_link:
    mov si, 0x1CE
    mov al, [sector_buf + si + 4]
    call drm_is_ext_type
    jnc .drm_ebr_done_clc
    call drm_entry_lba16_ok
    jnc .drm_ebr_done_clc
    mov ax, word ptr [drm_ext_base]
    add ax, [sector_buf + si + 8]
    jc .drm_ebr_done_clc
    cmp ax, word ptr [drm_ebr_lba]
    je .drm_ebr_done_clc
    mov word ptr [drm_ebr_lba], ax
    jmp .drm_ebr_loop

.drm_ebr_done_clc:
    clc
    jmp .drm_ebr_ret
.drm_ebr_full:
    stc
.drm_ebr_ret:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* AX=LBA, BX=buffer offset. Uses drm_bios_dl / drm_spt / drm_heads. */
drm_read_lba:
    push ax
    push bx
    push cx
    push dx
    push si
    mov si, bx
    xor dx, dx
    div word ptr [drm_spt]
    mov cl, dl
    inc cl
    xor dx, dx
    div word ptr [drm_heads]
    mov dh, dl
    mov ch, al
    mov al, ah
    mov ah, cl
    mov cl, 6
    shl al, cl
    or al, ah
    mov cl, al
    mov dl, byte ptr [drm_bios_dl]
    mov bx, si
    mov ax, 0x0201
    int 0x13
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* AL=type. CF set if DOS FAT type. */
drm_is_dos_type:
    cmp al, 0x01
    je .drm_dos_y
    cmp al, 0x04
    je .drm_dos_y
    cmp al, 0x06
    je .drm_dos_y
    clc
    ret
.drm_dos_y:
    stc
    ret

/* AL=type. CF set if extended container. */
drm_is_ext_type:
    cmp al, 0x05
    je .drm_ext_y
    cmp al, 0x0F
    je .drm_ext_y
    clc
    ret
.drm_ext_y:
    stc
    ret

/* SI → partition entry. CF set if start LBA fits in 16 bits and nonzero. */
drm_entry_lba16_ok:
    cmp word ptr [sector_buf + si + 10], 0
    jne .drm_lba_no
    cmp word ptr [sector_buf + si + 8], 0
    je .drm_lba_no
    stc
    ret
.drm_lba_no:
    clc
    ret

/* IN: DL ignored (use drm_bios_dl for map), AX=base_lba. CF if full. DS=CS. */
drm_push:
    push bx
    push dx
    mov dl, byte ptr [drm_bios_dl]
    mov bl, [num_drives]
    cmp bl, DRIVEMAP_MAX
    jae .drm_push_full
    xor bh, bh
    mov [drive_map_bios + bx], dl
    push ax
    add bx, bx
    pop ax
    mov [drive_map_base + bx], ax
    inc byte ptr [num_drives]
    clc
    pop dx
    pop bx
    ret
.drm_push_full:
    stc
    pop dx
    pop bx
    ret

/*
 * Save CS:cwd_cluster / cwd_path into drive_cwd_*[cur_drive].
 */
dos_cwd_save_current:
    push ax
    push bx
    push cx
    push si
    push di
    push es
    push ds
    push cs
    pop ds
    push cs
    pop es
    mov bl, [cur_drive]
    xor bh, bh
    cmp bl, CDS_COUNT
    jae .dcs_done
    mov ax, [cwd_cluster]
    mov si, bx
    add si, si
    mov [drive_cwd_cluster + si], ax
    mov ax, bx
    mov cl, 6
    shl ax, cl
    lea di, [drive_cwd_path]
    add di, ax
    lea si, [cwd_path]
    mov cx, 64
    rep movsb
.dcs_done:
    pop ds
    pop es
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    ret

/*
 * After rebuild: set cur_drive / vol_want_base for CS:boot_drive.
 */
dos_bind_boot_drive:
    push ax
    push bx
    push dx
    push ds
    push cs
    pop ds
    mov dl, [boot_drive]
    xor bx, bx
.dbb_scan:
    cmp bl, [num_drives]
    jae .dbb_a
    cmp [drive_map_bios + bx], dl
    je .dbb_hit
    inc bx
    jmp .dbb_scan
.dbb_a:
    xor bx, bx
.dbb_hit:
    mov [cur_drive], bl
    mov al, [drive_map_bios + bx]
    mov [boot_drive], al
    xor bh, bh
    add bx, bx
    mov ax, [drive_map_base + bx]
    mov [vol_want_base], ax
    pop ds
    pop dx
    pop bx
    pop ax
    ret
