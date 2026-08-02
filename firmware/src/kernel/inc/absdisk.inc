/*
 * INT 25h / 26h — absolute disk read / write (logical sectors).
 * AL=drive, CX=count, DX=start sector, DS:BX=buffer.
 * With CX=FFFFh, DS:BX points to the DOS 3.31 packet:
 *   +0 DWORD start sector, +4 WORD count, +6 DWORD buffer (off, seg).
 * Returns via RETF leaving original FLAGS on the caller's stack.
 */

install_absdisk_vectors:
    push ax
    push bx
    push es
    xor ax, ax
    mov es, ax
    lea bx, [int25_handler]
    mov word ptr es:[0x25 * 4], bx
    mov word ptr es:[0x25 * 4 + 2], cs
    lea bx, [int26_handler]
    mov word ptr es:[0x26 * 4], bx
    mov word ptr es:[0x26 * 4 + 2], cs
    pop es
    pop bx
    pop ax
    ret

int25_handler:
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es
    mov byte ptr cs:[abs_write], 0
    call absdisk_xfer
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    retf

int26_handler:
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es
    mov byte ptr cs:[abs_write], 1
    call absdisk_xfer
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    retf

/*
 * IN: AL=drive, CX=count, DX=start, DS:BX=buf; CS:abs_write.
 *     CX=FFFFh selects the DOS 3.31 packet described above.
 * OUT: CF clear + AX=0 on success; CF set + AX=error on failure.
 *
 * Sector transfers that would cross a physical 64K boundary use CS:sector_buf
 * as a bounce buffer (INT 13h / DMA cannot safely span a page).
 */
absdisk_xfer:
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es

    mov byte ptr cs:[abs_saved_drv], 0xFF
    mov ah, byte ptr cs:[cur_drive]
    mov byte ptr cs:[abs_saved_drv], ah
    /*
     * Decode arguments before dos_select_drive clobbers DX via DL.
     * During the loop DS holds the high word of the logical sector; the
     * caller's DS has already been saved and ES holds the transfer segment.
     */
    mov si, cx
    mov di, dx
    mov bp, bx
    push ds
    pop es
    xor dx, dx
    cmp cx, 0xFFFF
    jne .ax_args_ready
    mov di, word ptr [bx]
    mov dx, word ptr [bx + 2]
    mov si, word ptr [bx + 4]
    mov bp, word ptr [bx + 6]
    mov es, word ptr [bx + 8]
.ax_args_ready:
    mov ds, dx
    push ax
    mov dl, al
    call dos_select_drive
    pop ax
    jc .ax_drv

.ax_loop:
    test si, si
    jz .ax_ok
    /* Physical offset in 64K page: (ES*16 + BP) & 0xFFFF */
    mov ax, es
    mov cl, 4
    shl ax, cl
    add ax, bp
    cmp ax, 0xFE00
    ja .ax_bounce
    /* Also bounce if ES:BP offset itself wraps within the segment. */
    mov ax, bp
    cmp ax, 0xFE00
    ja .ax_bounce

    mov ax, di
    mov dx, ds
    add ax, word ptr cs:[vol_base_lba]
    adc dx, 0
    mov bx, bp
    cmp byte ptr cs:[abs_write], 0
    jne .ax_wr
    call fat12_read_lba
    jmp .ax_one
.ax_wr:
    call fat12_write_lba
.ax_one:
    jc .ax_io
.ax_next:
    add bp, 512
    inc di
    jnz .ax_lba_next
    mov ax, ds
    inc ax
    mov ds, ax
.ax_lba_next:
    dec si
    jmp .ax_loop

.ax_bounce:
    cmp byte ptr cs:[abs_write], 0
    jne .ax_bounce_wr
    /* Read into sector_buf, then copy to ES:BP */
    push es
    push cs
    pop es
    lea bx, [sector_buf]
    mov ax, di
    mov dx, ds
    add ax, word ptr cs:[vol_base_lba]
    adc dx, 0
    call fat12_read_lba
    pop es
    jc .ax_io
    push ds
    push si
    push di
    push cs
    pop ds
    lea si, [sector_buf]
    mov di, bp
    mov cx, 256
    cld
    rep movsw
    pop di
    pop si
    pop ds
    jmp .ax_next

.ax_bounce_wr:
    /* Copy ES:BP → sector_buf, then write */
    mov ax, di
    mov dx, ds
    add ax, word ptr cs:[vol_base_lba]
    adc dx, 0
    push ds
    push si
    push di
    push es
    push es
    pop ds
    mov si, bp
    push cs
    pop es
    lea di, [sector_buf]
    mov cx, 256
    cld
    rep movsw
    pop es
    push es
    push cs
    pop es
    lea bx, [sector_buf]
    call fat12_write_lba
    pop es
    pop di
    pop si
    pop ds
    jc .ax_io
    jmp .ax_next

.ax_ok:
    call .ax_restore
    xor ax, ax
    clc
    jmp .ax_done

.ax_io:
    call .ax_restore
    mov ax, 0x0207
    stc
    jmp .ax_done

.ax_drv:
    call .ax_restore
    mov ax, 0x0201
    stc

.ax_done:
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ret

.ax_restore:
    push ax
    push dx
    mov dl, byte ptr cs:[abs_saved_drv]
    cmp dl, 0xFF
    je .axr_skip
    call dos_select_drive
.axr_skip:
    pop dx
    pop ax
    ret
