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
    cmp ah, 0x04
    je .i10_read_light_pen
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
    cmp ah, 0x0C
    je .i10_write_pixel
    cmp ah, 0x0D
    je .i10_read_pixel
    cmp ah, 0x0E
    je .i10_teletype
    cmp ah, 0x0F
    je .i10_get_mode
    cmp ah, 0x13
    je .i10_write_string
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
    push ax
    push dx
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov [BDA_CURSOR_TYPE], cx
    /* Program CRTC cursor start/end (0Ah/0Bh). */
    mov dx, [BDA_CRT_PORT]
    mov al, 0x0A
    out dx, al
    inc dx
    mov al, ch
    out dx, al
    dec dx
    mov al, 0x0B
    out dx, al
    inc dx
    mov al, cl
    out dx, al
    pop ds
    pop dx
    pop ax
    iret

.i10_set_cursor:
    push ax
    push bx
    push cx
    push dx
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov cl, bh                   /* CL = requested page */
    mov bl, bh
    xor bh, bh
    shl bx, 1
    mov [BDA_CURSOR_POS + bx], dx
    cmp cl, [BDA_CRT_PAGE]
    jne .i10_sc_skip
    call crtc_set_cursor_addr    /* DX = row/col, DS = BDA */
.i10_sc_skip:
    pop ds
    pop dx
    pop cx
    pop bx
    pop ax
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

/* AH=04h — read light pen; AH=0 means not triggered / absent */
.i10_read_light_pen:
    xor ah, ah
    iret

.i10_set_page:
    push ax
    push bx
    push cx
    push dx
    push ds
    mov cl, al
    mov ax, BDA_SEG
    mov ds, ax
    /*
     * CGA regeneration memory is 16KB.  The page length gives 8 pages
     * in 40-column text modes, 4 in 80-column modes, and 1 in graphics.
     */
    mov ax, 0x4000
    xor dx, dx
    div word ptr [BDA_CRT_LEN]
    dec al
    cmp cl, al
    jbe .i10_sp_valid
    mov cl, al
.i10_sp_valid:
    mov [BDA_CRT_PAGE], cl

    /* BDA/CRTC start addresses are character, not byte, offsets. */
    xor ch, ch
    mov ax, cx
    mov bx, [BDA_CRT_LEN]
    shr bx, 1
    mul bx
    mov [BDA_CRT_START], ax
    call crtc_set_start_addr

    /* Display the selected page's cursor in text modes. */
    cmp byte ptr [BDA_CRT_MODE], 4
    jae .i10_sp_done
    mov bx, cx
    shl bx, 1
    mov dx, [BDA_CURSOR_POS + bx]
    call crtc_set_cursor_addr
.i10_sp_done:
    pop ds
    pop dx
    pop cx
    pop bx
    pop ax
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
    push ax
    mov ax, BDA_SEG
    mov es, ax
    cmp byte ptr es:[BDA_CRT_MODE], 4
    pop ax
    jb .i10_scroll_up_text
    call gfx_scroll_up
    jmp .i10_scroll_up_done
.i10_scroll_up_text:
    call video_scroll_up
.i10_scroll_up_done:
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
    push ax
    mov ax, BDA_SEG
    mov es, ax
    cmp byte ptr es:[BDA_CRT_MODE], 4
    pop ax
    jb .i10_scroll_dn_text
    call gfx_scroll_dn
    jmp .i10_scroll_dn_done
.i10_scroll_dn_text:
    call video_scroll_dn
.i10_scroll_dn_done:
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
    push bx
    push cx
    push dx
    mov di, BDA_SEG
    mov ds, di
    cmp byte ptr [BDA_CRT_MODE], 4
    jae .i10_rc_gfx
    mov al, bh
    mov bl, bh
    xor bh, bh
    shl bx, 1
    mov dx, [BDA_CURSOR_POS + bx]
    call cursor_to_page_offset
    mov ax, CGA_SEG
    mov es, ax
    mov ax, es:[di]
    jmp .i10_rc_done
.i10_rc_gfx:
    call gfx_read_char           /* AL=matched char, AH=0 */
.i10_rc_done:
    pop dx
    pop cx
    pop bx
    pop di
    pop es
    pop ds
    iret

.i10_write_char:
    push ds
    push es
    push di
    push si
    push ax
    push bx
    push cx
    push dx
    mov di, BDA_SEG
    mov ds, di
    cmp byte ptr [BDA_CRT_MODE], 4
    jae .i10_wc_gfx
    push bx
    mov bl, bh
    xor bh, bh
    shl bx, 1
    mov dx, [BDA_CURSOR_POS + bx]
    pop bx
    mov al, bh
    call cursor_to_page_offset
    mov ax, CGA_SEG
    mov es, ax
    pop dx
    pop cx
    pop bx
    pop ax
    mov ah, bl
    jcxz .i10_wc_done
.i10_wc_loop:
    mov es:[di], ax
    add di, 2
    loop .i10_wc_loop
    jmp .i10_wc_done
.i10_wc_gfx:
    /* AL=char, BL=color, CX=count; write at cursor without advancing. */
    pop dx
    pop cx                       /* count */
    pop bx
    pop ax                       /* AL=char */
    mov si, cx
    jcxz .i10_wc_done
    mov cl, bl                   /* color → CL for gfx_tty_glyph */
.i10_wc_gfx_loop:
    push ax
    push bx
    push cx
    push si
    call gfx_tty_glyph
    pop si
    pop cx
    pop bx
    pop ax
    dec si
    jnz .i10_wc_gfx_loop
.i10_wc_done:
    pop si
    pop di
    pop es
    pop ds
    iret

.i10_write_char_only:
    push ds
    push es
    push di
    push si
    push ax
    push bx
    push cx
    push dx
    mov di, BDA_SEG
    mov ds, di
    cmp byte ptr [BDA_CRT_MODE], 4
    jae .i10_wco_gfx
    push bx
    mov bl, bh
    xor bh, bh
    shl bx, 1
    mov dx, [BDA_CURSOR_POS + bx]
    pop bx
    mov al, bh
    call cursor_to_page_offset
    mov ax, CGA_SEG
    mov es, ax
    pop dx
    pop cx
    pop bx
    pop ax
    jcxz .i10_wco_done
.i10_wco_loop:
    mov es:[di], al
    add di, 2
    loop .i10_wco_loop
    jmp .i10_wco_done
.i10_wco_gfx:
    /* Same plane write as AH=09 (BL supplies color in graphics). */
    pop dx
    pop cx
    pop bx
    pop ax
    mov si, cx
    jcxz .i10_wco_done
    mov cl, bl
.i10_wco_gfx_loop:
    push ax
    push bx
    push cx
    push si
    call gfx_tty_glyph
    pop si
    pop cx
    pop bx
    pop ax
    dec si
    jnz .i10_wco_gfx_loop
.i10_wco_done:
    pop si
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

/* AH=0Ch write pixel: AL=color, CX=X, DX=Y */
.i10_write_pixel:
    push ax
    push bx
    push cx
    push dx
    push di
    push es
    push ds
    mov bx, BDA_SEG
    mov ds, bx
    mov bl, [BDA_CRT_MODE]
    cmp bl, 4
    jb .i10_wp_done
    mov bx, dx                   /* BX = Y */
    mov dx, cx                   /* DX = X */
    cmp byte ptr [BDA_CRT_MODE], 6
    je .i10_wp_m6
    call cga_plot_m4
    jmp .i10_wp_done
.i10_wp_m6:
    call cga_plot_m6
.i10_wp_done:
    pop ds
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    iret

/* AH=0Dh read pixel: CX=X, DX=Y → AL */
.i10_read_pixel:
    push bx
    push cx
    push dx
    push di
    push es
    push ds
    mov bx, BDA_SEG
    mov ds, bx
    xor al, al
    cmp byte ptr [BDA_CRT_MODE], 4
    jb .i10_rp_done
    mov bx, dx
    mov dx, cx
    cmp byte ptr [BDA_CRT_MODE], 6
    je .i10_rp_m6
    call cga_read_m4
    jmp .i10_rp_done
.i10_rp_m6:
    call cga_read_m6
.i10_rp_done:
    pop ds
    pop es
    pop di
    pop dx
    pop cx
    pop bx
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
    je .i10_bel

    cmp byte ptr [BDA_CRT_MODE], 4
    jb .i10_tt_text
    call gfx_tty_glyph           /* AL=char, CL=color; uses cursor BDA */
    jmp .i10_tt_advance

.i10_tt_text:
    mov bl, [BDA_CRT_PAGE]
    xor bh, bh
    shl bx, 1
    mov dx, [BDA_CURSOR_POS + bx]
    push ax
    call cursor_to_offset
    mov bx, CGA_SEG
    mov es, bx
    pop ax
    mov ah, 0x07
    mov es:[di], ax

.i10_tt_advance:
    mov bl, [BDA_CRT_PAGE]
    xor bh, bh
    shl bx, 1
    inc byte ptr [BDA_CURSOR_POS + bx]
    mov al, byte ptr [BDA_CRT_COLS]
    cmp byte ptr [BDA_CURSOR_POS + bx], al
    jb .i10_tt_done
    mov byte ptr [BDA_CURSOR_POS + bx], 0
    jmp .i10_lf
.i10_cr:
    mov bl, [BDA_CRT_PAGE]
    xor bh, bh
    shl bx, 1
    mov byte ptr [BDA_CURSOR_POS + bx], 0
    jmp .i10_tt_done
.i10_lf:
    mov bl, [BDA_CRT_PAGE]
    xor bh, bh
    shl bx, 1
    inc byte ptr [BDA_CURSOR_POS + bx + 1]
    cmp byte ptr [BDA_CURSOR_POS + bx + 1], 25
    jb .i10_tt_done
    mov byte ptr [BDA_CURSOR_POS + bx + 1], 24
    cmp byte ptr [BDA_CRT_MODE], 4
    jae .i10_lf_gfx
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
.i10_lf_gfx:
    call gfx_scroll_up_row
    jmp .i10_tt_done
.i10_bs:
    mov bl, [BDA_CRT_PAGE]
    xor bh, bh
    shl bx, 1
    cmp byte ptr [BDA_CURSOR_POS + bx], 0
    je .i10_tt_done
    dec byte ptr [BDA_CURSOR_POS + bx]
    jmp .i10_tt_done
.i10_bel:
    call speaker_beep
.i10_tt_done:
    /* Keep CRTC cursor in sync for text modes. */
    cmp byte ptr [BDA_CRT_MODE], 4
    jae .i10_tt_iret
    mov bl, [BDA_CRT_PAGE]
    xor bh, bh
    shl bx, 1
    mov dx, [BDA_CURSOR_POS + bx]
    call crtc_set_cursor_addr
.i10_tt_iret:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    pop es
    pop ds
    iret

/*
 * AH=13h write string: ES:BP = text, CX = length, DH/DL = row/col,
 * BH = page, BL = attribute (AL=0/1). AL bit0 = update cursor after write.
 * AL=2/3: ES:BP is char+attr pairs (CX = pair count); BL unused.
 */
.i10_write_string:
    push ds
    push es
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp

    mov si, bp                   /* ES:SI = string */
    mov bp, ax
    and bp, 3                    /* bit0=update cursor, bit1=char+attr pairs */

    mov ax, BDA_SEG
    mov ds, ax

    mov al, bh
    xor ah, ah
    mov di, ax
    shl di, 1                    /* DI = page cursor slot */

    push word ptr [BDA_CURSOR_POS + di]
    mov [BDA_CURSOR_POS + di], dx

    jcxz .i10_ws_finish

    cmp byte ptr [BDA_CRT_MODE], 4
    jae .i10_ws_gfx

    mov al, bh
    call cursor_to_page_offset   /* DI = regen byte offset */
    push ds                      /* BDA */
    push es                      /* string seg */
    mov ax, CGA_SEG
    mov es, ax
    pop ds                       /* DS:SI = string */

.i10_ws_loop:
    test bp, 2
    jnz .i10_ws_pair
    lodsb
    mov ah, bl
    jmp .i10_ws_store
.i10_ws_pair:
    lodsw                        /* AL=char AH=attr */
.i10_ws_store:
    stosw                        /* ES:DI char+attr; DI += 2 */
    loop .i10_ws_loop

    pop ds                       /* DS = BDA */
    jmp .i10_ws_finish

.i10_ws_gfx:
    /* Graphics plane string: plot glyphs and advance BDA cursor. */
    push es
    pop ds                       /* DS:SI = string; ES was string seg */
    mov ax, BDA_SEG
    mov es, ax                   /* ES = BDA for cursor updates */
.i10_ws_gfx_loop:
    test bp, 2
    jnz .i10_ws_gfx_pair
    lodsb
    mov ah, bl                   /* color/attr */
    jmp .i10_ws_gfx_plot
.i10_ws_gfx_pair:
    lodsw                        /* AL=char AH=color */
.i10_ws_gfx_plot:
    push ds
    push es
    push ax
    push bx
    push cx
    push bp
    push si
    mov bx, BDA_SEG
    mov ds, bx
    mov cl, ah                   /* color */
    call gfx_tty_glyph
    /* Advance cursor one column (wrap). */
    mov al, [BDA_CURSOR_POS]
    inc al
    cmp al, [BDA_CRT_COLS]
    jb .i10_ws_gfx_setc
    xor al, al
    inc byte ptr [BDA_CURSOR_POS + 1]
.i10_ws_gfx_setc:
    mov [BDA_CURSOR_POS], al
    pop si
    pop bp
    pop cx
    pop bx
    pop ax
    pop es
    pop ds
    loop .i10_ws_gfx_loop
    mov ax, BDA_SEG
    mov ds, ax

.i10_ws_finish:
    mov al, bh
    xor ah, ah
    mov di, ax
    shl di, 1

    pop dx                       /* prior cursor */
    test bp, 1
    jz .i10_ws_restore

    /* Cursor after last character (wrap columns). */
    mov bp, sp
    /* stack: bp di si dx cx bx ax es ds */
    mov cx, word ptr [bp + 8]
    mov ax, word ptr [bp + 6]
    add al, cl
    mov bl, byte ptr [BDA_CRT_COLS]
.i10_ws_wrap:
    cmp al, bl
    jb .i10_ws_set
    sub al, bl
    inc ah
    jmp .i10_ws_wrap
.i10_ws_set:
    mov [BDA_CURSOR_POS + di], ax
    cmp byte ptr [BDA_CRT_MODE], 4
    jae .i10_ws_done
    mov dx, ax
    call crtc_set_cursor_addr
    jmp .i10_ws_done
.i10_ws_restore:
    mov [BDA_CURSOR_POS + di], dx
.i10_ws_done:
    pop bp
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
 * Resolve 8×8 glyph for AL → ES:SI.
 * 00–7F: F000:FA6E; 80–FF: INT 1Fh table + (AL-80h)*8.
 * Clobbers AX,BX.
 */
gfx_font_ptr:
    mov ah, 0
    mov bx, ax
    test al, 0x80
    jnz .gfp_hi
    shl bx, 1
    shl bx, 1
    shl bx, 1                    /* BX = AL*8 */
    mov ax, BIOS_SEG
    mov es, ax
    mov si, 0xFA6E
    add si, bx
    ret
.gfp_hi:
    and bx, 0x7F
    shl bx, 1
    shl bx, 1
    shl bx, 1
    push ds
    xor ax, ax
    mov ds, ax
    les si, [0x1F * 4]
    pop ds
    add si, bx
    ret

/*
 * Plot 8×8 glyph AL at BDA cursor in CGA graphics (modes 4–6).
 * CL = foreground (low bits); bit7 → XOR. DS = BDA on entry.
 * Clobbers AX,BX,DX,SI,DI,ES.
 */
gfx_tty_glyph:
    push bp
    mov bp, sp
    push cx                      /* [bp-2] color */

    call gfx_font_ptr            /* ES:SI → glyph */

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
    push si
    push es
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
    pop es
    pop si
    pop bx
    pop dx
.gtg45_draw:
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
 * Read graphics cell at BDA cursor by matching against ROM/INT1Fh fonts.
 * DS=BDA. OUT: AL=char (0 if no match), AH=0. Clobbers BX,CX,DX,SI,DI,ES.
 * Uses BDA 40:AC..40:B3 as an 8-byte capture scratch (XT-unused).
 */
gfx_read_char:
    push ds
    mov ax, BDA_SEG
    mov ds, ax

    mov al, [BDA_CURSOR_POS]
    mov ah, 0
    mov si, ax
    shl si, 1
    shl si, 1
    shl si, 1
    mov al, [BDA_CURSOR_POS + 1]
    mov ah, 0
    mov bx, ax
    shl bx, 1
    shl bx, 1
    shl bx, 1

    mov di, 0xAC
    mov cx, 8
.grc_cap_row:
    push cx
    push bx
    xor ah, ah
    mov dx, si
    mov cx, 8
.grc_cap_pix:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    cmp byte ptr [BDA_CRT_MODE], 6
    je .grc_rd6
    call cga_read_m4
    jmp .grc_rd_have
.grc_rd6:
    call cga_read_m6
.grc_rd_have:
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    test al, al                  /* AL still holds pixel color */
    pop ax                       /* restore row builder in AH */
    pushf
    shl ah, 1
    popf
    jz .grc_cap_next
    or ah, 1
.grc_cap_next:
    inc dx
    loop .grc_cap_pix
    mov [di], ah
    inc di
    pop bx
    inc bx
    pop cx
    loop .grc_cap_row

    /* Prefer non-blank matches: scan 1..255 then 0. */
    mov bx, 1
.grc_match:
    mov al, bl
    push bx
    call gfx_font_ptr
    pop bx
    mov di, 0xAC
    mov cx, 8
.grc_cmp:
    mov al, es:[si]
    cmp al, [di]
    jne .grc_next
    inc si
    inc di
    loop .grc_cmp
    mov al, bl
    xor ah, ah
    pop ds
    ret
.grc_next:
    inc bx
    cmp bx, 256
    jb .grc_match
    xor al, al
    call gfx_font_ptr
    mov di, 0xAC
    mov cx, 8
.grc_blank:
    mov al, es:[si]
    cmp al, [di]
    jne .grc_nomatch
    inc si
    inc di
    loop .grc_blank
    xor ax, ax
    pop ds
    ret
.grc_nomatch:
    xor ax, ax
    pop ds
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
 * Read mode-4/5 pixel. DX=X, BX=Y → AL=color 0..3.
 * Clobbers BX,CX,DX,DI,ES.
 */
cga_read_m4:
    push dx
    mov di, bx
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
    pop bx
    mov ax, bx
    shr ax, 1
    shr ax, 1
    add di, ax
    and bx, 3
    xor bx, 3
    shl bx, 1
    mov al, es:[di]
    mov cl, bl
    shr al, cl
    and al, 3
    ret

/*
 * Read mode-6 pixel. DX=X, BX=Y → AL=0/1.
 */
cga_read_m6:
    push dx
    mov di, bx
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
    pop bx
    mov ax, bx
    shr ax, 1
    shr ax, 1
    shr ax, 1
    add di, ax
    and bl, 7
    mov cl, 7
    sub cl, bl
    mov al, es:[di]
    shr al, cl
    and al, 1
    ret

/*
 * Scroll CGA graphics up one glyph row (8 scanlines) in both banks.
 * Clobbers AX,CX,SI,DI,ES,DS.
 */
gfx_scroll_up_row:
    push ax
    push cx
    push si
    push di
    push ds
    push es
    mov ax, CGA_SEG
    mov ds, ax
    mov es, ax
    /* even bank: move 96 lines * 80 bytes up by 4 even lines (320 bytes) */
    mov si, 320
    xor di, di
    mov cx, 7680
    rep movsb
    xor ax, ax
    mov cx, 320
    rep stosb
    /* odd bank at 0x2000 */
    mov si, 0x2000 + 320
    mov di, 0x2000
    mov cx, 7680
    rep movsb
    xor ax, ax
    mov cx, 320
    rep stosb
    pop es
    pop ds
    pop di
    pop si
    pop cx
    pop ax
    ret

/*
 * Scroll a CGA graphics character window. Each character row is eight pixel
 * scanlines, represented by four contiguous 80-byte lines in each CGA bank.
 * Mode 4/5 columns span two bytes; mode 6 columns span one byte.
 * IN: AL=rows (0=clear), CH/CL=upper-left, DH/DL=lower-right.
 */
gfx_scroll_up:
    xor si, si
    jmp gfx_scroll_window

gfx_scroll_dn:
    mov si, 1

gfx_scroll_window:
    push bp
    mov bp, sp
    sub sp, 24
    mov word ptr [bp - 2], ax    /* input AX */
    mov word ptr [bp - 4], bx    /* input BX */
    mov word ptr [bp - 6], cx    /* input CX */
    mov word ptr [bp - 8], dx    /* input DX */
    mov word ptr [bp - 24], si   /* 0=up, 1=down */
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

    mov ax, BDA_SEG
    mov ds, ax
    mov al, byte ptr [BDA_CRT_MODE]
    cmp al, 4
    jb .gsw_done
    cmp al, 6
    ja .gsw_done

    /* Clamp the character rectangle to the 25-row graphics screen. */
    mov al, byte ptr [bp - 5]    /* CH */
    cmp al, 24
    ja .gsw_done
    mov ah, byte ptr [bp - 7]    /* DH */
    cmp ah, 24
    jbe .gsw_rows_clamped
    mov ah, 24
    mov byte ptr [bp - 7], ah
.gsw_rows_clamped:
    cmp al, ah
    ja .gsw_done

    mov bl, byte ptr [BDA_CRT_COLS]
    mov al, byte ptr [bp - 6]    /* CL */
    cmp al, bl
    jae .gsw_done
    mov ah, byte ptr [bp - 8]    /* DL */
    cmp ah, bl
    jb .gsw_cols_clamped
    mov ah, bl
    dec ah
    mov byte ptr [bp - 8], ah
.gsw_cols_clamped:
    cmp al, ah
    ja .gsw_done

    mov dl, 1                    /* bytes per character column */
    cmp byte ptr [BDA_CRT_MODE], 6
    je .gsw_bpc_ready
    mov dl, 2
.gsw_bpc_ready:
    mov al, byte ptr [bp - 6]
    mul dl
    mov word ptr [bp - 10], ax   /* x byte offset */
    mov al, byte ptr [bp - 8]
    sub al, byte ptr [bp - 6]
    inc al
    mul dl
    mov word ptr [bp - 12], ax   /* width in bytes */

    mov al, byte ptr [bp - 5]
    xor ah, ah
    shl ax, 1
    shl ax, 1
    mov word ptr [bp - 14], ax   /* first per-bank scanline */
    mov al, byte ptr [bp - 7]
    sub al, byte ptr [bp - 5]
    inc al
    xor ah, ah
    shl ax, 1
    shl ax, 1
    mov word ptr [bp - 16], ax   /* per-bank height */

    mov al, byte ptr [bp - 2]
    xor ah, ah
    test ax, ax
    jz .gsw_clear_all
    shl ax, 1
    shl ax, 1
    cmp ax, word ptr [bp - 16]
    jb .gsw_shift_ready
.gsw_clear_all:
    mov ax, word ptr [bp - 16]
.gsw_shift_ready:
    mov word ptr [bp - 18], ax   /* per-bank shift */
    mov dx, word ptr [bp - 16]
    sub dx, ax
    mov word ptr [bp - 20], dx   /* lines to copy */

    mov ax, CGA_SEG
    mov ds, ax
    mov es, ax
    cld
    cmp word ptr [bp - 24], 0
    jne .gsw_down_start

    mov word ptr [bp - 22], 0
.gsw_up_bank:
    mov ax, word ptr [bp - 14]
    mov bx, 80
    mul bx
    add ax, word ptr [bp - 10]
    add ax, word ptr [bp - 22]
    mov di, ax
    mov si, ax
    mov ax, word ptr [bp - 18]
    mov bx, 80
    mul bx
    add si, ax
    mov dx, word ptr [bp - 20]
.gsw_up_copy:
    test dx, dx
    jz .gsw_up_clear_start
    mov cx, word ptr [bp - 12]
    rep movsb
    mov ax, 80
    sub ax, word ptr [bp - 12]
    add si, ax
    add di, ax
    dec dx
    jmp .gsw_up_copy
.gsw_up_clear_start:
    mov dx, word ptr [bp - 18]
.gsw_up_clear:
    test dx, dx
    jz .gsw_up_next_bank
    xor al, al
    mov cx, word ptr [bp - 12]
    rep stosb
    mov ax, 80
    sub ax, word ptr [bp - 12]
    add di, ax
    dec dx
    jmp .gsw_up_clear
.gsw_up_next_bank:
    cmp word ptr [bp - 22], 0
    jne .gsw_done
    mov word ptr [bp - 22], 0x2000
    jmp .gsw_up_bank

.gsw_down_start:
    mov word ptr [bp - 22], 0
.gsw_down_bank:
    mov ax, word ptr [bp - 14]
    add ax, word ptr [bp - 16]
    dec ax
    mov bx, 80
    mul bx
    add ax, word ptr [bp - 10]
    add ax, word ptr [bp - 22]
    mov di, ax
    mov si, ax
    mov ax, word ptr [bp - 18]
    mov bx, 80
    mul bx
    sub si, ax
    mov dx, word ptr [bp - 20]
.gsw_down_copy:
    test dx, dx
    jz .gsw_down_clear_start
    mov cx, word ptr [bp - 12]
    rep movsb
    mov ax, 80
    add ax, word ptr [bp - 12]
    sub si, ax
    sub di, ax
    dec dx
    jmp .gsw_down_copy
.gsw_down_clear_start:
    mov dx, word ptr [bp - 18]
.gsw_down_clear:
    test dx, dx
    jz .gsw_down_next_bank
    xor al, al
    mov cx, word ptr [bp - 12]
    rep stosb
    mov ax, 80
    add ax, word ptr [bp - 12]
    sub di, ax
    dec dx
    jmp .gsw_down_clear
.gsw_down_next_bank:
    cmp word ptr [bp - 22], 0
    jne .gsw_done
    mov word ptr [bp - 22], 0x2000
    jmp .gsw_down_bank

.gsw_done:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    mov sp, bp
    pop bp
    ret

/*
 * DS=BDA. AX=character start address. Program CRTC start registers 0Ch/0Dh.
 * Clobbers AX,DX.
 */
crtc_set_start_addr:
    push ax
    push bx
    push dx
    mov bx, ax
    mov dx, [BDA_CRT_PORT]
    mov al, 0x0C
    out dx, al
    inc dx
    mov al, bh
    out dx, al
    dec dx
    mov al, 0x0D
    out dx, al
    inc dx
    mov al, bl
    out dx, al
    pop dx
    pop bx
    pop ax
    ret

/*
 * DS=BDA. DX=row/col. Program CRTC cursor address registers 0Eh/0Fh.
 * Clobbers AX,BX,DX (port).
 */
crtc_set_cursor_addr:
    push ax
    push bx
    push cx
    push dx
    push di
    call cursor_to_offset        /* DI = byte offset in regen for active page */
    shr di, 1                    /* character offset within page */
    mov bx, di                   /* BX = CRTC cursor address */
    mov dx, [BDA_CRT_PORT]
    mov al, 0x0E
    out dx, al
    inc dx
    mov al, bh
    out dx, al
    dec dx
    mov al, 0x0F
    out dx, al
    inc dx
    mov al, bl
    out dx, al
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* Short PIT ch2 + PPI speaker beep (BEL). Clobbers AX,CX,DX. */
speaker_beep:
    push ax
    push cx
    push dx
    mov al, 0xB6
    out PORT_PIT_MODE, al
    mov ax, 0x0533               /* ~880 Hz */
    out PORT_PIT_CH2, al
    mov al, ah
    out PORT_PIT_CH2, al
    in al, PORT_PPI_B
    mov ah, al
    or al, 0x03
    out PORT_PPI_B, al
    mov cx, 0x3000
.spk_wait:
    loop .spk_wait
    mov al, ah
    and al, 0xFC
    out PORT_PPI_B, al
    pop dx
    pop cx
    pop ax
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
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov al, [BDA_CRT_PAGE]
    pop ds
    call cursor_to_page_offset
    pop ax
    ret

/*
 * DX=row/col, AL=page.  Return DI=regen byte offset for that text page.
 * Clobbers DI only.
 */
cursor_to_page_offset:
    push bp
    mov bp, sp
    push ax
    push bx
    push dx
    push ds
    mov bl, al
    xor bh, bh
    mov ax, BDA_SEG
    mov ds, ax
    mov ax, bx
    mov bx, [BDA_CRT_LEN]
    mul bx                       /* AX = page byte base */
    push ax
    mov dx, [bp - 6]             /* restore input row/column after MUL */
    mov al, dh
    xor ah, ah
    mov bl, byte ptr [BDA_CRT_COLS]
    mul bl
    mov bl, dl
    xor bh, bh
    add ax, bx
    shl ax, 1
    add ax, [bp - 10]
    add sp, 2
    mov di, ax
    pop ds
    pop dx
    pop bx
    pop ax
    pop bp
    ret
