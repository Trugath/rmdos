.code16
.intel_syntax noprefix

.include "firmware/bios/inc/equates.inc"

/*
 * rmDOS clean-room XT system BIOS — CGA video:
 * mode set, INT 10h services, graphics teletype, scroll helpers.
 */

.section .text
.global init_cga, int10_handler

init_cga:
    /* Default POST: mode 3 (80x25 color text). */
    mov al, 0x03
    call set_video_mode
    ret

/*
 * Program CGA for mode AL (0..6). Updates BDA + CRTC + 3D8/3D9 + clears VRAM.
 * Clobbers AX,BX,CX,DX,SI,DI,ES.
 */
set_video_mode:
    push ds
    push cs
    pop ds                      /* DS = CS for tables */

    and al, 0x7F
    cmp al, 6
    jbe .svm_ok
    mov al, 3
.svm_ok:
    mov bl, al
    xor bh, bh                  /* BX = mode */

    mov ax, BDA_SEG
    mov es, ax
    mov es:[BDA_CRT_MODE], bl
    mov byte ptr es:[BDA_CRT_PAGE], 0
    mov word ptr es:[BDA_CRT_START], 0
    mov word ptr es:[BDA_CURSOR_POS], 0
    mov word ptr es:[BDA_CURSOR_TYPE], 0x0607

    mov al, [vid_cols + bx]
    xor ah, ah
    mov es:[BDA_CRT_COLS], ax

    mov al, [vid_len_lo + bx]
    mov ah, [vid_len_hi + bx]
    mov es:[BDA_CRT_LEN], ax

    mov al, [vid_mode_ctl + bx]
    mov es:[BDA_CRT_MODE_REG], al
    mov dx, PORT_CGA_MODE
    out dx, al

    mov al, [vid_color + bx]
    mov es:[BDA_CRT_PALETTE], al
    mov dx, PORT_CGA_COLOR
    out dx, al

    /* CRTC: 16 regs at vid_crtc + mode*16 */
    mov al, bl
    mov cl, 4
    shl al, cl
    xor ah, ah
    mov si, offset vid_crtc
    add si, ax
    xor bl, bl
.svm_crtc:
    mov dx, PORT_CRTC_IDX
    mov al, bl
    out dx, al
    mov dx, PORT_CRTC_DATA
    lodsb
    out dx, al
    inc bl
    cmp bl, 16
    jb .svm_crtc

    /* clear regen: graphics → 16KB zero; text → page length of 0x0720 */
    mov ax, CGA_SEG
    mov es, ax
    xor di, di
    mov ax, BDA_SEG
    mov ds, ax
    mov al, [BDA_CRT_MODE]
    cmp al, 4
    jb .svm_clr_text
    mov cx, 0x2000
    xor ax, ax
    rep stosw
    jmp .svm_done
.svm_clr_text:
    mov cx, [BDA_CRT_LEN]
    shr cx, 1
    mov ax, 0x0720
    rep stosw
.svm_done:
    pop ds
    ret

vid_cols:
    .byte 40, 40, 80, 80, 40, 40, 80
vid_len_lo:
    .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
vid_len_hi:
    .byte 0x08, 0x08, 0x10, 0x10, 0x40, 0x40, 0x40
vid_mode_ctl:
    /* 3D8: modes 0..6 */
    .byte 0x2C, 0x28, 0x2D, 0x29, 0x2A, 0x2E, 0x1E
vid_color:
    .byte 0x3F, 0x30, 0x3F, 0x30, 0x30, 0x30, 0x3F

vid_crtc:
    /* 0 — 40×25 */
    .byte 0x38,0x28,0x2D,0x0A,0x1F,0x06,0x19,0x1C,0x02,0x07,0x06,0x07,0x00,0x00,0x00,0x00
    /* 1 */
    .byte 0x38,0x28,0x2D,0x0A,0x1F,0x06,0x19,0x1C,0x02,0x07,0x06,0x07,0x00,0x00,0x00,0x00
    /* 2 — 80×25 */
    .byte 0x71,0x50,0x5A,0x0A,0x1F,0x06,0x19,0x1C,0x02,0x07,0x06,0x07,0x00,0x00,0x00,0x00
    /* 3 */
    .byte 0x71,0x50,0x5A,0x0A,0x1F,0x06,0x19,0x1C,0x02,0x07,0x06,0x07,0x00,0x00,0x00,0x00
    /* 4 — 320×200 */
    .byte 0x38,0x28,0x2D,0x0A,0x7F,0x06,0x64,0x70,0x02,0x01,0x06,0x07,0x00,0x00,0x00,0x00
    /* 5 */
    .byte 0x38,0x28,0x2D,0x0A,0x7F,0x06,0x64,0x70,0x02,0x01,0x06,0x07,0x00,0x00,0x00,0x00
    /* 6 — 640×200 */
    .byte 0x38,0x28,0x2D,0x0A,0x7F,0x06,0x64,0x70,0x02,0x01,0x06,0x07,0x00,0x00,0x00,0x00

int10_handler:
    sti
    cmp ah, 0x00
    je .i10_set_mode
    cmp ah, 0x01
    je .i10_set_ctype
    cmp ah, 0x02
    je .i10_set_cursor
    cmp ah, 0x03
    je .i10_get_cursor
    cmp ah, 0x05
    je .i10_set_page
    cmp ah, 0x06
    je .i10_scroll_up
    cmp ah, 0x07
    je .i10_scroll_dn
    cmp ah, 0x08
    je .i10_read_char
    cmp ah, 0x09
    je .i10_write_char
    cmp ah, 0x0A
    je .i10_write_char_only
    cmp ah, 0x0B
    je .i10_set_palette
    cmp ah, 0x0E
    je .i10_teletype
    cmp ah, 0x0F
    je .i10_get_mode
    iret

.i10_set_mode:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    call set_video_mode
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    iret

.i10_set_ctype:
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov [BDA_CURSOR_TYPE], cx
    pop ds
    iret

.i10_set_cursor:
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov bl, bh
    xor bh, bh
    shl bx, 1
    mov [BDA_CURSOR_POS + bx], dx
    pop ds
    iret

.i10_get_cursor:
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov bl, bh
    xor bh, bh
    shl bx, 1
    mov dx, [BDA_CURSOR_POS + bx]
    mov cx, [BDA_CURSOR_TYPE]
    pop ds
    iret

.i10_set_page:
    push ds
    mov bx, BDA_SEG
    mov ds, bx
    mov [BDA_CRT_PAGE], al
    pop ds
    iret

.i10_scroll_up:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    call video_scroll_up
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    iret

.i10_scroll_dn:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    call video_scroll_dn
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    iret

.i10_read_char:
    push ds
    push es
    push di
    mov di, BDA_SEG
    mov ds, di
    mov bl, bh
    xor bh, bh
    shl bx, 1
    mov dx, [BDA_CURSOR_POS + bx]
    call cursor_to_offset
    mov ax, CGA_SEG
    mov es, ax
    mov ax, es:[di]
    pop di
    pop es
    pop ds
    iret

.i10_write_char:
    push ds
    push es
    push di
    push ax
    push cx
    mov di, BDA_SEG
    mov ds, di
    push bx
    mov bl, bh
    xor bh, bh
    shl bx, 1
    mov dx, [BDA_CURSOR_POS + bx]
    pop bx
    call cursor_to_offset
    mov ax, CGA_SEG
    mov es, ax
    pop cx
    pop ax
    mov ah, bl
    jcxz .i10_wc_done
.i10_wc_loop:
    mov es:[di], ax
    add di, 2
    loop .i10_wc_loop
.i10_wc_done:
    pop di
    pop es
    pop ds
    iret

.i10_write_char_only:
    push ds
    push es
    push di
    push ax
    push cx
    mov di, BDA_SEG
    mov ds, di
    push bx
    mov bl, bh
    xor bh, bh
    shl bx, 1
    mov dx, [BDA_CURSOR_POS + bx]
    pop bx
    call cursor_to_offset
    mov ax, CGA_SEG
    mov es, ax
    pop cx
    pop ax
    jcxz .i10_wco_done
.i10_wco_loop:
    mov es:[di], al
    add di, 2
    loop .i10_wco_loop
.i10_wco_done:
    pop di
    pop es
    pop ds
    iret

.i10_set_palette:
    /* BH=0: set border/bg from BL low nibble; BH=1: select palette (BL&1)<<5 into 3D9 */
    push ds
    push ax
    push dx
    mov ax, BDA_SEG
    mov ds, ax
    mov al, [BDA_CRT_PALETTE]
    cmp bh, 0
    jne .i10_pal_sel
    and al, 0xE0
    mov ah, bl
    and ah, 0x1F
    or al, ah
    jmp .i10_pal_out
.i10_pal_sel:
    and al, 0xDF
    test bl, 1
    jz .i10_pal_out
    or al, 0x20
.i10_pal_out:
    mov [BDA_CRT_PALETTE], al
    mov dx, PORT_CGA_COLOR
    out dx, al
    pop dx
    pop ax
    pop ds
    iret

.i10_teletype:
    push ds
    push es
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov cx, bx                   /* CL = graphics fg color (BL) */
    mov bx, BDA_SEG
    mov ds, bx
    cmp al, 0x0D
    je .i10_cr
    cmp al, 0x0A
    je .i10_lf
    cmp al, 0x08
    je .i10_bs
    cmp al, 0x07
    je .i10_tt_done

    cmp byte ptr [BDA_CRT_MODE], 4
    jb .i10_tt_text
    call gfx_tty_glyph           /* AL=char, CL=color; uses cursor BDA */
    jmp .i10_tt_advance

.i10_tt_text:
    mov dx, [BDA_CURSOR_POS]
    push ax
    call cursor_to_offset
    mov bx, CGA_SEG
    mov es, bx
    pop ax
    mov ah, 0x07
    mov es:[di], ax

.i10_tt_advance:
    inc byte ptr [BDA_CURSOR_POS]
    mov al, byte ptr [BDA_CRT_COLS]
    cmp byte ptr [BDA_CURSOR_POS], al
    jb .i10_tt_done
    mov byte ptr [BDA_CURSOR_POS], 0
    jmp .i10_lf
.i10_cr:
    mov byte ptr [BDA_CURSOR_POS], 0
    jmp .i10_tt_done
.i10_lf:
    inc byte ptr [BDA_CURSOR_POS + 1]
    cmp byte ptr [BDA_CURSOR_POS + 1], 25
    jb .i10_tt_done
    mov byte ptr [BDA_CURSOR_POS + 1], 24
    /* scroll: text path only for now */
    cmp byte ptr [BDA_CRT_MODE], 4
    jae .i10_tt_done
    push ax
    push bx
    push cx
    push dx
    mov ax, 0x0601
    mov bh, 0x07
    xor cx, cx
    mov dx, 0x184F
    call video_scroll_up
    pop dx
    pop cx
    pop bx
    pop ax
    jmp .i10_tt_done
.i10_bs:
    cmp byte ptr [BDA_CURSOR_POS], 0
    je .i10_tt_done
    dec byte ptr [BDA_CURSOR_POS]
.i10_tt_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    pop es
    pop ds
    iret

.i10_get_mode:
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov ah, [BDA_CRT_COLS]
    mov al, [BDA_CRT_MODE]
    mov bh, [BDA_CRT_PAGE]
    pop ds
    iret

/*
 * Plot 8×8 glyph AL at BDA cursor in CGA graphics (modes 4–6).
 * CL = foreground (low bits); bit7 → XOR. DS = BDA on entry.
 * Clobbers AX,BX,DX,SI,DI,ES.
 */
gfx_tty_glyph:
    push bp
    mov bp, sp
    push cx                      /* [bp-2] color */

    and al, 0x7F                 /* ROM font is 0..127 */
    mov ah, 0
    mov bx, ax
    shl bx, 1
    shl bx, 1
    shl bx, 1                    /* BX = AL*8 */
    mov ax, BIOS_SEG
    mov es, ax
    mov si, 0xFA6E
    add si, bx                   /* ES:SI → glyph */

    mov al, [BDA_CURSOR_POS]     /* col */
    mov ah, 0
    mov dx, ax
    shl dx, 1
    shl dx, 1
    shl dx, 1                    /* DX = pixel X = col*8 */
    mov al, [BDA_CURSOR_POS + 1]
    mov ah, 0
    mov bx, ax
    shl bx, 1
    shl bx, 1
    shl bx, 1                    /* BX = pixel Y = row*8 */

    mov al, [BDA_CRT_MODE]
    cmp al, 6
    je .gtg_mode6

    /* Mode 4/5: clear 8×8 cell unless XOR, then plot */
    test byte ptr [bp - 2], 0x80
    jnz .gtg45_draw
    push dx
    push bx
    mov cx, 8
.gtg45_clr_y:
    push cx
    mov cx, 8
.gtg45_clr_x:
    push ax
    push bx
    push cx
    push dx
    xor al, al
    call cga_plot_m4
    pop dx
    pop cx
    pop bx
    pop ax
    inc dx
    loop .gtg45_clr_x
    sub dx, 8
    inc bx
    pop cx
    loop .gtg45_clr_y
    pop bx
    pop dx
.gtg45_draw:
    /* cga_plot_m4 leaves ES=B800 — restore font segment */
    mov ax, BIOS_SEG
    mov es, ax
    mov cx, 8
.gtg45_scan:
    push cx
    mov ah, es:[si]              /* one glyph row (MSB = left) */
    inc si
    push si
    push es

    mov cx, 8                    /* 8 pixels */
.gtg45_pix:
    shl ah, 1
    jnc .gtg45_skip
    push ax
    push bx
    push cx
    push dx
    mov al, [bp - 2]             /* color */
    call cga_plot_m4
    pop dx
    pop cx
    pop bx
    pop ax
.gtg45_skip:
    inc dx                       /* next X */
    loop .gtg45_pix

    sub dx, 8                    /* rewind X */
    inc bx                       /* next Y */
    pop es
    pop si
    pop cx
    loop .gtg45_scan
    jmp .gtg_done

.gtg_mode6:
    mov cx, 8
.gtg6_scan:
    push cx
    mov ah, es:[si]
    inc si
    push si
    push es
    mov cx, 8
.gtg6_pix:
    shl ah, 1
    jnc .gtg6_skip
    push ax
    push bx
    push cx
    push dx
    mov al, [bp - 2]
    call cga_plot_m6
    pop dx
    pop cx
    pop bx
    pop ax
.gtg6_skip:
    inc dx
    loop .gtg6_pix
    sub dx, 8
    inc bx
    pop es
    pop si
    pop cx
    loop .gtg6_scan

.gtg_done:
    mov sp, bp
    pop bp
    ret

/*
 * Plot one mode-4/5 pixel. DX=X, BX=Y, AL=color (bit7=XOR).
 * Clobbers AX,BX,CX,DX,DI,ES.
 */
cga_plot_m4:
    push ax                      /* color flags (don't keep in AH — AX reused) */
    push dx                      /* X */
    mov di, bx                   /* DI = Y */

    mov ax, CGA_SEG
    mov es, ax

    mov bx, di
    and bx, 1
    mov cl, 13
    shl bx, cl                   /* bank */
    shr di, 1
    /* DI = Y/2 * 80 = Y/2*64 + Y/2*16 */
    mov ax, di
    mov cl, 6
    shl ax, cl                   /* *64 */
    mov cl, 4
    shl di, cl                   /* *16 */
    add di, ax
    add di, bx                   /* + bank */

    pop bx                       /* BX = X */
    mov ax, bx
    shr ax, 1
    shr ax, 1
    add di, ax                   /* byte offset */

    and bx, 3
    xor bx, 3                    /* 3-(X&3) */
    shl bx, 1                    /* shift count */

    pop ax                       /* color */
    mov ah, al
    and al, 3
    mov cl, bl
    shl al, cl
    mov dl, 3
    shl dl, cl

    test ah, 0x80
    jnz .cpm4_xor
    not dl
    and es:[di], dl
    or es:[di], al
    ret
.cpm4_xor:
    xor es:[di], al
    ret

/*
 * Plot one mode-6 pixel. DX=X, BX=Y, AL=color (bit0 on; bit7=XOR).
 */
cga_plot_m6:
    push ax                      /* color */
    push dx                      /* X */
    mov di, bx                   /* Y */

    mov ax, CGA_SEG
    mov es, ax

    mov bx, di
    and bx, 1
    mov cl, 13
    shl bx, cl
    shr di, 1
    mov ax, di
    mov cl, 6
    shl ax, cl
    mov cl, 4
    shl di, cl
    add di, ax
    add di, bx

    pop bx                       /* X */
    mov ax, bx
    shr ax, 1
    shr ax, 1
    shr ax, 1
    add di, ax

    and bl, 7
    mov cl, 7
    sub cl, bl
    mov dl, 1
    shl dl, cl

    pop ax                       /* color */
    test al, 0x80
    jnz .cpm6_xor
    test al, 1
    jz .cpm6_clear
    or es:[di], dl
    ret
.cpm6_clear:
    not dl
    and es:[di], dl
    ret
.cpm6_xor:
    xor es:[di], dl
    ret

/*
 * Scroll helpers.
 * IN: AL=lines (0=clear window), BH=fill attr,
 *     CH/CL = upper-left row/col, DH/DL = lower-right row/col
 * Clobbers AX,BX,CX,DX,SI,DI,ES.
 */
video_scroll_up:
    push bp
    mov bp, sp
    sub sp, 10
    mov [bp - 2], ax            /* AL = lines */
    mov [bp - 4], bx            /* BH = attr */
    mov [bp - 6], cx            /* CH/CL UL */
    mov [bp - 8], dx            /* DH/DL LR */

    mov ax, CGA_SEG
    mov es, ax

    /* height = DH - CH + 1 → [bp-10] */
    mov al, [bp - 7]            /* DH */
    sub al, [bp - 5]            /* CH */
    inc al
    xor ah, ah
    mov [bp - 10], ax

    mov al, [bp - 2]            /* lines */
    test al, al
    jz .vsu_fill
    cmp al, [bp - 10]
    jae .vsu_fill

    /* copy upward: dest_row from CH to DH-lines */
    mov dh, [bp - 5]            /* dest row = CH */
.vsu_copy:
    mov al, [bp - 7]            /* DH */
    sub al, [bp - 2]            /* lines */
    cmp dh, al
    ja .vsu_fill_bot
    mov ah, dh
    add ah, [bp - 2]            /* src row */
    mov dl, [bp - 6]            /* col = CL */
.vsu_copy_col:
    cmp dl, [bp - 8]
    ja .vsu_copy_next
    push dx
    mov dh, ah                  /* read from src row */
    call cursor_to_offset
    mov bx, es:[di]
    pop dx
    push ax
    push dx
    call cursor_to_offset
    pop dx
    pop ax
    mov es:[di], bx
    inc dl
    jmp .vsu_copy_col
.vsu_copy_next:
    inc dh
    jmp .vsu_copy

.vsu_fill_bot:
    /* fill bottom `lines` rows: rows (DH-lines+1) .. DH */
    mov dh, [bp - 7]
    sub dh, [bp - 2]
    inc dh
    jmp .vsu_fill_from

.vsu_fill:
    mov dh, [bp - 5]            /* from CH */
.vsu_fill_from:
    mov bh, [bp - 3]            /* attr */
    mov ah, bh
    mov al, ' '
.vsu_fill_row:
    cmp dh, [bp - 7]
    ja .vsu_done
    mov dl, [bp - 6]
.vsu_fill_col:
    cmp dl, [bp - 8]
    ja .vsu_fill_next
    push ax
    push dx
    call cursor_to_offset
    pop dx
    pop ax
    mov es:[di], ax
    inc dl
    jmp .vsu_fill_col
.vsu_fill_next:
    inc dh
    jmp .vsu_fill_row
.vsu_done:
    mov sp, bp
    pop bp
    ret

video_scroll_dn:
    push bp
    mov bp, sp
    sub sp, 10
    mov [bp - 2], ax
    mov [bp - 4], bx
    mov [bp - 6], cx
    mov [bp - 8], dx
    mov ax, CGA_SEG
    mov es, ax

    mov al, [bp - 7]
    sub al, [bp - 5]
    inc al
    xor ah, ah
    mov [bp - 10], ax

    mov al, [bp - 2]
    test al, al
    jz .vsd_fill
    cmp al, [bp - 10]
    jae .vsd_fill

    /* copy downward: dest_row from DH down to CH+lines */
    mov dh, [bp - 7]
.vsd_copy:
    mov al, [bp - 5]
    add al, [bp - 2]
    cmp dh, al
    jb .vsd_fill_top
    mov ah, dh
    sub ah, [bp - 2]            /* src row */
    mov dl, [bp - 6]
.vsd_copy_col:
    cmp dl, [bp - 8]
    ja .vsd_copy_next
    push dx
    mov dh, ah
    call cursor_to_offset
    mov bx, es:[di]
    pop dx
    push ax
    push dx
    call cursor_to_offset
    pop dx
    pop ax
    mov es:[di], bx
    inc dl
    jmp .vsd_copy_col
.vsd_copy_next:
    dec dh
    jmp .vsd_copy

.vsd_fill_top:
    mov dh, [bp - 5]
    mov al, [bp - 5]
    add al, [bp - 2]
    dec al
    mov [bp - 7], al            /* temporarily LR row = CH+lines-1 */
    jmp .vsd_fill_from

.vsd_fill:
    mov dh, [bp - 5]
.vsd_fill_from:
    mov bh, [bp - 3]
    mov ah, bh
    mov al, ' '
.vsd_fill_row:
    cmp dh, [bp - 7]
    ja .vsd_done
    mov dl, [bp - 6]
.vsd_fill_col:
    cmp dl, [bp - 8]
    ja .vsd_fill_next
    push ax
    push dx
    call cursor_to_offset
    pop dx
    pop ax
    mov es:[di], ax
    inc dl
    jmp .vsd_fill_col
.vsd_fill_next:
    inc dh
    jmp .vsd_fill_row
.vsd_done:
    mov sp, bp
    pop bp
    ret

cursor_to_offset:
    push ax
    push bx
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov al, dh
    xor ah, ah
    mov bl, byte ptr [BDA_CRT_COLS]
    mul bl
    mov bl, dl
    xor bh, bh
    add ax, bx
    shl ax, 1
    mov di, ax
    pop ds
    pop bx
    pop ax
    ret
