/*
 * Logical drive map: letter → (BIOS DL, partition base LBA).
 * A:/B: are floppies; hard-disk DOS primaries (01/04/06) get C: onward
 * in slot order per disk; whole-disk FAT VBR gets one letter at LBA 0.
 */

.equ DRIVEMAP_MAX, 8

/*
 * Rebuild CS:drive_map / num_drives. Uses CS:sector_buf.
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
    push dx
    mov ah, 0x08
    int 0x13
    pop dx
    jc .drm_done

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

    xor bx, bx
    mov si, 0x1BE
    mov cx, 4
.drm_cnt:
    mov al, [sector_buf + si + 4]
    cmp al, 0x01
    je .drm_cnt_dos
    cmp al, 0x04
    je .drm_cnt_dos
    cmp al, 0x06
    jne .drm_cnt_next
.drm_cnt_dos:
    cmp word ptr [sector_buf + si + 10], 0
    jne .drm_cnt_next
    cmp word ptr [sector_buf + si + 8], 0
    je .drm_cnt_next
    inc bx
.drm_cnt_next:
    add si, 16
    loop .drm_cnt
    test bx, bx
    jz .drm_try_vbr

    mov si, 0x1BE
    mov cx, 4
.drm_add:
    mov al, [sector_buf + si + 4]
    cmp al, 0x01
    je .drm_add_dos
    cmp al, 0x04
    je .drm_add_dos
    cmp al, 0x06
    jne .drm_add_next
.drm_add_dos:
    cmp word ptr [sector_buf + si + 10], 0
    jne .drm_add_next
    mov ax, [sector_buf + si + 8]
    test ax, ax
    jz .drm_add_next
    call drm_push
    jc .drm_done
.drm_add_next:
    add si, 16
    loop .drm_add
    jmp .drm_next_disk

.drm_try_vbr:
    cmp word ptr [sector_buf + 11], 512
    jne .drm_next_disk
    xor ax, ax
    call drm_push
    jc .drm_done

.drm_next_disk:
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

/* IN: DL=bios, AX=base_lba. OUT: CF if map full. DS=CS. */
drm_push:
    push bx
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
    pop bx
    ret
.drm_push_full:
    stc
    pop bx
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
