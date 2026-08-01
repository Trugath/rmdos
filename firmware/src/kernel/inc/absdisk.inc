/*
 * INT 25h / 26h — absolute disk read / write (logical sectors).
 * AL=drive, CX=count, DX=start sector, DS:BX=buffer.
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
 * IN: AL=drive, CX=count, DX=start, DS:BX=buf; CS:abs_write
 * OUT: CF clear + AX=0 on success; CF set + AX=error on failure.
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
    push ax
    mov dl, al
    call dos_select_drive
    pop ax
    jc .ax_drv

    mov si, cx
    mov di, dx
    mov bp, bx
    push ds
    pop es

.ax_loop:
    test si, si
    jz .ax_ok
    mov ax, di
    xor dx, dx
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
    add bp, 512
    inc di
    dec si
    jmp .ax_loop

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
