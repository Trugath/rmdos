/*
 * INT 21h FCB subset: AH=0F/10/11/12/13/14/15/16/17/21/22/27/28/29.
 *
 * FCB (DS:DX; AH=29 uses ES:DI + DS:SI):
 *   +00 drive (0=default, 1=A)  +01 name8  +09 ext3
 *   +0C cur block  +0E record size  +10 file size
 *   +14 date  +16 time  +18 handle  +19 open flag
 *   +20 cur record (0-127)  +21 relative record (24-bit)
 *   +11 new name (AH=17 rename only)
 *
 * Path via path.inc; open/create/R/W/close via handle INT 21h (files.inc);
 * find via find.inc using CS:fcb_find_dta.
 */

.equ FCB_DRV, 0
.equ FCB_NAME, 1
.equ FCB_NEWNAME, 0x11
.equ FCB_BLOCK, 0x0C
.equ FCB_RECSIZ, 0x0E
.equ FCB_FSIZE, 0x10
.equ FCB_DATE, 0x14
.equ FCB_TIME, 0x16
.equ FCB_HANDLE, 0x18
.equ FCB_OPEN, 0x19
.equ FCB_CURREC, 0x20
.equ FCB_RELREC, 0x21

/*
 * CS:fcb_path from FCB at DS:SI. Mounts drive. CF if drive bad.
 */
fcb_build_path:
    push ax
    push bx
    push cx
    push dx
    push di
    push es

    mov al, [si + FCB_DRV]
    test al, al
    jz .fbp_def
    dec al
    jmp .fbp_have
.fbp_def:
    mov al, byte ptr cs:[cur_drive]
.fbp_have:
    cmp al, byte ptr cs:[num_drives]
    jae .fbp_bad
    mov dl, al
    push si
    call dos_mount_drive
    pop si
    jc .fbp_bad

    push cs
    pop es
    lea di, [fcb_path]
    add al, 'A'
    stosb
    mov al, ':'
    stosb

    lea bx, [si + FCB_NAME]
    mov cx, 8
.fbp_stem:
    mov al, [bx]
    cmp al, ' '
    je .fbp_stem_skip
    stosb
    inc bx
    loop .fbp_stem
    jmp .fbp_do_ext
.fbp_stem_skip:
    add bx, cx
.fbp_do_ext:
    cmp byte ptr [bx], ' '
    je .fbp_z
    mov al, '.'
    stosb
    mov cx, 3
.fbp_ext:
    mov al, [bx]
    cmp al, ' '
    je .fbp_z
    stosb
    inc bx
    loop .fbp_ext
.fbp_z:
    xor al, al
    stosb
    clc
    jmp .fbp_done
.fbp_bad:
    stc
.fbp_done:
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* CF set if CS:fcb_path has ? or *. */
fcb_path_wild:
    push ax
    push si
    push ds
    push cs
    pop ds
    lea si, [fcb_path]
.fpw:
    lodsb
    test al, al
    jz .fpw_no
    cmp al, '?'
    je .fpw_yes
    cmp al, '*'
    je .fpw_yes
    jmp .fpw
.fpw_no:
    clc
    jmp .fpw_done
.fpw_yes:
    stc
.fpw_done:
    pop ds
    pop si
    pop ax
    ret

/*
 * Byte offset -> DX:AX from sequential fields at DS:SI.
 * CF if record size is 0.
 */
fcb_byte_pos:
    push bx
    push cx
    push si
    mov ax, [si + FCB_BLOCK]
    mov cx, 128
    mul cx
    mov bl, [si + FCB_CURREC]
    xor bh, bh
    add ax, bx
    adc dx, 0
    mov cx, [si + FCB_RECSIZ]
    jcxz .fbpos_bad
    mov bx, dx
    mul cx
    mov si, ax
    mov ax, bx
    mov bx, dx
    mul cx
    add ax, bx
    mov dx, ax
    mov ax, si
    pop si
    clc
    jmp .fbpos_done
.fbpos_bad:
    pop si
    stc
.fbpos_done:
    pop cx
    pop bx
    ret

fcb_advance_seq:
    inc byte ptr [si + FCB_CURREC]
    cmp byte ptr [si + FCB_CURREC], 128
    jb .fas_rel
    mov byte ptr [si + FCB_CURREC], 0
    inc word ptr [si + FCB_BLOCK]
.fas_rel:
    add word ptr [si + FCB_RELREC], 1
    adc byte ptr [si + FCB_RELREC + 2], 0
    ret

/*
 * Byte offset DX:AX from relative record at DS:SI.
 * CF if record size is 0.
 */
fcb_rel_byte_pos:
    push bx
    push cx
    push si
    mov ax, [si + FCB_RELREC]
    mov dl, [si + FCB_RELREC + 2]
    xor dh, dh
    mov cx, [si + FCB_RECSIZ]
    jcxz .frbp_bad
    mov bx, dx
    mul cx
    mov si, ax
    mov ax, bx
    mov bx, dx
    mul cx
    add ax, bx
    mov dx, ax
    mov ax, si
    pop si
    clc
    jmp .frbp_done
.frbp_bad:
    pop si
    stc
.frbp_done:
    pop cx
    pop bx
    ret

fcb_advance_rel:
    add word ptr [si + FCB_RELREC], 1
    adc byte ptr [si + FCB_RELREC + 2], 0
    /* keep seq fields loosely in sync */
    inc byte ptr [si + FCB_CURREC]
    cmp byte ptr [si + FCB_CURREC], 128
    jb .far_done
    mov byte ptr [si + FCB_CURREC], 0
    inc word ptr [si + FCB_BLOCK]
.far_done:
    ret

/* Init open FCB fields at DS:SI; BL = handle. */
fcb_init_open:
    mov [si + FCB_HANDLE], bl
    mov byte ptr [si + FCB_OPEN], 1
    cmp word ptr [si + FCB_RECSIZ], 0
    jne .fio_pos
    mov word ptr [si + FCB_RECSIZ], 128
.fio_pos:
    mov word ptr [si + FCB_BLOCK], 0
    mov byte ptr [si + FCB_CURREC], 0
    mov word ptr [si + FCB_RELREC], 0
    mov byte ptr [si + FCB_RELREC + 2], 0
    ret

/* Handle BX open: fill size/date/time at DS:SI. */
fcb_query_meta:
    push ax
    push cx
    push dx
    mov ax, 0x4202
    xor cx, cx
    xor dx, dx
    int 0x21
    jc .fqm_err
    mov [si + FCB_FSIZE], ax
    mov [si + FCB_FSIZE + 2], dx
    mov ax, 0x4200
    xor cx, cx
    xor dx, dx
    int 0x21
    jc .fqm_err
    mov ax, 0x5700
    int 0x21
    jc .fqm_ok
    mov [si + FCB_TIME], cx
    mov [si + FCB_DATE], dx
.fqm_ok:
    clc
    jmp .fqm_done
.fqm_err:
    stc
.fqm_done:
    pop dx
    pop cx
    pop ax
    ret

/* AH=0Fh open FCB -> AL=00/FF */
dos_ah0f:
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

    mov si, dx
    call fcb_build_path
    jc .a0f_fail
    call fcb_path_wild
    jc .a0f_fail

    push ds
    push cs
    pop ds
    lea dx, [fcb_path]
    mov ax, 0x3D02
    int 0x21
    pop ds
    jc .a0f_fail
    mov bx, ax
    call fcb_query_meta
    jc .a0f_clo
    call fcb_init_open
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    xor al, al
    clc
    jmp int21_iret
.a0f_clo:
    mov ah, 0x3E
    int 0x21
.a0f_fail:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov al, 0xFF
    clc
    jmp int21_iret

/* AH=10h close FCB */
dos_ah10:
    push bx
    push dx
    push si
    mov si, dx
    cmp byte ptr [si + FCB_OPEN], 0
    je .a10_fail
    mov bl, [si + FCB_HANDLE]
    xor bh, bh
    mov ah, 0x3E
    int 0x21
    jc .a10_fail
    mov byte ptr [si + FCB_OPEN], 0
    pop si
    pop dx
    pop bx
    xor al, al
    clc
    jmp int21_iret
.a10_fail:
    pop si
    pop dx
    pop bx
    mov al, 0xFF
    clc
    jmp int21_iret

/* AH=16h create FCB */
dos_ah16:
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

    mov si, dx
    call fcb_build_path
    jc .a16_fail
    call fcb_path_wild
    jc .a16_fail

    push ds
    push cs
    pop ds
    lea dx, [fcb_path]
    xor cx, cx
    mov ah, 0x3C
    int 0x21
    pop ds
    jc .a16_fail
    mov bx, ax
    xor ax, ax
    mov [si + FCB_FSIZE], ax
    mov [si + FCB_FSIZE + 2], ax
    mov [si + FCB_DATE], ax
    mov [si + FCB_TIME], ax
    call fcb_init_open
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    xor al, al
    clc
    jmp int21_iret
.a16_fail:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov al, 0xFF
    clc
    jmp int21_iret

/* AH=13h delete FCB -> AL=00/FF */
dos_ah13:
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

    mov si, dx
    call fcb_build_path
    jc .a13_fail
    call fcb_path_wild
    jc .a13_fail

    push ds
    push cs
    pop ds
    lea dx, [fcb_path]
    mov ah, 0x41
    int 0x21
    pop ds
    jc .a13_fail
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    xor al, al
    clc
    jmp int21_iret
.a13_fail:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov al, 0xFF
    clc
    jmp int21_iret

/*
 * AH=17h rename FCB. New 11-byte name at FCB+11h.
 * Builds old/new ASCIZ and calls AH=56.
 */
dos_ah17:
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push bp

    mov si, dx
    call fcb_build_path
    jc .a17_fail
    call fcb_path_wild
    jc .a17_fail

    push cs
    pop es
    lea di, [fcb_path2]
    lea si, [fcb_path]
    push ds
    push cs
    pop ds
    mov cx, 16
    rep movsb
    pop ds

    mov si, dx
    push cx
    mov cx, 11
    lea di, [si + FCB_NAME]
    lea bx, [si + FCB_NEWNAME]
.a17_swap:
    mov al, [di]
    mov ah, [bx]
    mov [di], ah
    mov [bx], al
    inc di
    inc bx
    loop .a17_swap
    pop cx

    mov si, dx
    call fcb_build_path
    pushf
    /* restore name fields */
    mov cx, 11
    lea di, [si + FCB_NAME]
    lea bx, [si + FCB_NEWNAME]
.a17_unswap:
    mov al, [di]
    mov ah, [bx]
    mov [di], ah
    mov [bx], al
    inc di
    inc bx
    loop .a17_unswap
    popf
    jc .a17_fail
    call fcb_path_wild
    jc .a17_fail

    push cs
    pop ds
    lea dx, [fcb_path2]
    push cs
    pop es
    lea di, [fcb_path]
    mov ah, 0x56
    int 0x21
    jc .a17_fail_rest
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    xor al, al
    clc
    jmp int21_iret
.a17_fail_rest:
.a17_fail:
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov al, 0xFF
    clc
    jmp int21_iret

/* AH=21h random read one record */
dos_ah21:
    mov cx, 1
    jmp dos_ah27

/* AH=22h random write one record */
dos_ah22:
    mov cx, 1
    jmp dos_ah28

/*
 * AH=27h random block read. CX=count in/out; AL=00/01/03.
 * Uses relative record at +21h; transfers into DTA.
 */
dos_ah27:
    push bx
    push dx
    push si
    push di
    push ds
    push es
    push bp

    mov si, dx
    mov word ptr cs:[fcb_blk_want], cx
    mov word ptr cs:[fcb_blk_done], 0
    cmp byte ptr [si + FCB_OPEN], 0
    je .a27_fail
    mov ax, [si + FCB_RECSIZ]
    test ax, ax
    jz .a27_fail
    mov word ptr cs:[fcb_xfer], ax
    test cx, cx
    jnz .a27_loop
    jmp .a27_zero

.a27_loop:
    mov ax, word ptr cs:[fcb_blk_done]
    cmp ax, word ptr cs:[fcb_blk_want]
    jae .a27_done_ok

    call fcb_rel_byte_pos
    jc .a27_fail
    mov word ptr cs:[fcb_pos_lo], ax
    mov word ptr cs:[fcb_pos_hi], dx
    mov bx, [si + FCB_FSIZE]
    mov cx, [si + FCB_FSIZE + 2]
    cmp dx, cx
    ja .a27_eof
    jb .a27_room
    cmp ax, bx
    jae .a27_eof
.a27_room:
    sub bx, ax
    sbb cx, dx
    mov ax, word ptr cs:[fcb_xfer]
    xor bp, bp
    test cx, cx
    jnz .a27_want
    cmp bx, ax
    jae .a27_want
    mov ax, bx
    test ax, ax
    jz .a27_eof
    mov bp, 1
.a27_want:
    mov word ptr cs:[fcb_xfer], ax
    mov bl, [si + FCB_HANDLE]
    xor bh, bh
    mov ax, 0x4200
    mov cx, word ptr cs:[fcb_pos_hi]
    mov dx, word ptr cs:[fcb_pos_lo]
    int 0x21
    jc .a27_fail
    /* DTA + done*recsiz */
    mov ax, word ptr cs:[fcb_blk_done]
    mov cx, [si + FCB_RECSIZ]
    mul cx
    add ax, word ptr cs:[dta_off]
    mov dx, ax
    mov cx, word ptr cs:[fcb_xfer]
    mov ah, 0x3F
    push ds
    mov ds, word ptr cs:[dta_seg]
    int 0x21
    pop ds
    jc .a27_fail
    cmp ax, word ptr cs:[fcb_xfer]
    jb .a27_fail
    call fcb_advance_rel
    inc word ptr cs:[fcb_blk_done]
    test bp, bp
    jz .a27_loop
    mov al, 3
    jmp .a27_ret
.a27_eof:
    mov al, 1
    jmp .a27_ret
.a27_done_ok:
.a27_zero:
    xor al, al
.a27_ret:
    mov cx, word ptr cs:[fcb_blk_done]
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop bx
    clc
    jmp int21_iret
.a27_fail:
    mov cx, word ptr cs:[fcb_blk_done]
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop bx
    mov al, 1
    clc
    jmp int21_iret

/* AH=28h random block write. CX=count in/out; AL=00/01. */
dos_ah28:
    push bx
    push dx
    push si
    push di
    push ds
    push es

    mov si, dx
    mov word ptr cs:[fcb_blk_want], cx
    mov word ptr cs:[fcb_blk_done], 0
    cmp byte ptr [si + FCB_OPEN], 0
    je .a28_fail
    mov cx, [si + FCB_RECSIZ]
    test cx, cx
    jnz .a28_have_rec
    jmp .a28_fail
.a28_have_rec:
    mov word ptr cs:[fcb_xfer], cx
    mov cx, word ptr cs:[fcb_blk_want]
    test cx, cx
    jnz .a28_loop
    jmp .a28_zero

.a28_loop:
    mov ax, word ptr cs:[fcb_blk_done]
    cmp ax, word ptr cs:[fcb_blk_want]
    jae .a28_done_ok

    call fcb_rel_byte_pos
    jc .a28_fail
    mov word ptr cs:[fcb_pos_lo], ax
    mov word ptr cs:[fcb_pos_hi], dx
    mov bl, [si + FCB_HANDLE]
    xor bh, bh
    mov ax, 0x4200
    mov cx, word ptr cs:[fcb_pos_hi]
    mov dx, word ptr cs:[fcb_pos_lo]
    int 0x21
    jc .a28_fail
    mov ax, word ptr cs:[fcb_blk_done]
    mov cx, [si + FCB_RECSIZ]
    mul cx
    add ax, word ptr cs:[dta_off]
    mov dx, ax
    mov cx, word ptr cs:[fcb_xfer]
    mov ah, 0x40
    push ds
    mov ds, word ptr cs:[dta_seg]
    int 0x21
    pop ds
    jc .a28_fail
    cmp ax, word ptr cs:[fcb_xfer]
    jb .a28_fail
    mov ax, word ptr cs:[fcb_pos_lo]
    mov dx, word ptr cs:[fcb_pos_hi]
    add ax, word ptr cs:[fcb_xfer]
    adc dx, 0
    cmp dx, [si + FCB_FSIZE + 2]
    ja .a28_grow
    jb .a28_adv
    cmp ax, [si + FCB_FSIZE]
    jbe .a28_adv
.a28_grow:
    mov [si + FCB_FSIZE], ax
    mov [si + FCB_FSIZE + 2], dx
.a28_adv:
    call fcb_advance_rel
    inc word ptr cs:[fcb_blk_done]
    jmp .a28_loop
.a28_done_ok:
.a28_zero:
    xor al, al
    mov cx, word ptr cs:[fcb_blk_done]
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop bx
    clc
    jmp int21_iret
.a28_fail:
    mov cx, word ptr cs:[fcb_blk_done]
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop bx
    mov al, 1
    clc
    jmp int21_iret

/* AH=14h sequential read -> DTA; AL=00/01/03 */
dos_ah14:
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push bp

    mov si, dx
    cmp byte ptr [si + FCB_OPEN], 0
    je .a14_fail
    call fcb_byte_pos
    jc .a14_fail
    mov word ptr cs:[fcb_pos_lo], ax
    mov word ptr cs:[fcb_pos_hi], dx
    mov bx, [si + FCB_FSIZE]
    mov cx, [si + FCB_FSIZE + 2]
    cmp dx, cx
    ja .a14_eof
    jb .a14_room
    cmp ax, bx
    jae .a14_eof
.a14_room:
    sub bx, ax
    sbb cx, dx
    mov ax, [si + FCB_RECSIZ]
    test ax, ax
    jz .a14_fail
    xor bp, bp
    test cx, cx
    jnz .a14_want
    cmp bx, ax
    jae .a14_want
    mov ax, bx
    test ax, ax
    jz .a14_eof
    mov bp, 1
.a14_want:
    mov word ptr cs:[fcb_xfer], ax
    mov bl, [si + FCB_HANDLE]
    xor bh, bh
    mov ax, 0x4200
    mov cx, word ptr cs:[fcb_pos_hi]
    mov dx, word ptr cs:[fcb_pos_lo]
    int 0x21
    jc .a14_fail
    mov cx, word ptr cs:[fcb_xfer]
    mov ah, 0x3F
    push ds
    mov dx, word ptr cs:[dta_off]
    mov ds, word ptr cs:[dta_seg]
    int 0x21
    pop ds
    jc .a14_fail
    cmp ax, word ptr cs:[fcb_xfer]
    jb .a14_fail
    call fcb_advance_seq
    mov al, 0
    test bp, bp
    jz .a14_ok
    mov al, 3
.a14_ok:
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    clc
    jmp int21_iret
.a14_eof:
    mov al, 1
    jmp .a14_ok
.a14_fail:
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov al, 1
    clc
    jmp int21_iret

/* AH=15h sequential write from DTA; AL=00/01 */
dos_ah15:
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

    mov si, dx
    cmp byte ptr [si + FCB_OPEN], 0
    je .a15_fail
    call fcb_byte_pos
    jc .a15_fail
    mov word ptr cs:[fcb_pos_lo], ax
    mov word ptr cs:[fcb_pos_hi], dx
    mov cx, [si + FCB_RECSIZ]
    jcxz .a15_fail
    mov word ptr cs:[fcb_xfer], cx
    mov bl, [si + FCB_HANDLE]
    xor bh, bh
    mov ax, 0x4200
    mov cx, word ptr cs:[fcb_pos_hi]
    mov dx, word ptr cs:[fcb_pos_lo]
    int 0x21
    jc .a15_fail
    mov cx, word ptr cs:[fcb_xfer]
    mov ah, 0x40
    push ds
    mov dx, word ptr cs:[dta_off]
    mov ds, word ptr cs:[dta_seg]
    int 0x21
    pop ds
    jc .a15_fail
    cmp ax, word ptr cs:[fcb_xfer]
    jb .a15_fail
    mov ax, word ptr cs:[fcb_pos_lo]
    mov dx, word ptr cs:[fcb_pos_hi]
    add ax, word ptr cs:[fcb_xfer]
    adc dx, 0
    cmp dx, [si + FCB_FSIZE + 2]
    ja .a15_grow
    jb .a15_adv
    cmp ax, [si + FCB_FSIZE]
    jbe .a15_adv
.a15_grow:
    mov [si + FCB_FSIZE], ax
    mov [si + FCB_FSIZE + 2], dx
.a15_adv:
    call fcb_advance_seq
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    xor al, al
    clc
    jmp int21_iret
.a15_fail:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov al, 1
    clc
    jmp int21_iret

/* Copy 11-byte FCB name at DS:SI+1 into CS:find_pat. */
fcb_pattern_to_find:
    push ax
    push cx
    push si
    push di
    push es
    push ds
    push cs
    pop es
    lea di, [find_pat]
    add si, FCB_NAME
    mov cx, 11
    rep movsb
    pop ds
    pop es
    pop di
    pop si
    pop cx
    pop ax
    ret

/* Init CS:fcb_find_dta; DL = logical drive 0=A. */
fcb_find_init_dta:
    push ax
    push di
    push es
    push cs
    pop es
    lea di, [fcb_find_dta]
    mov ax, word ptr cs:[cwd_cluster]
    cmp dl, byte ptr cs:[cur_drive]
    je .ffi_cwd
    xor ax, ax
.ffi_cwd:
    mov es:[di], ax
    xor ax, ax
    mov word ptr es:[di + 2], ax
    mov byte ptr es:[di + 4], 0
    mov byte ptr es:[di + 5], 0
    mov byte ptr es:[di + 6], dl
    pop es
    pop di
    pop ax
    ret

fcb_use_find_dta:
    mov ax, word ptr cs:[dta_seg]
    mov word ptr cs:[fcb_saved_dta_seg], ax
    mov ax, word ptr cs:[dta_off]
    mov word ptr cs:[fcb_saved_dta_off], ax
    mov word ptr cs:[dta_seg], cs
    lea ax, [fcb_find_dta]
    mov word ptr cs:[dta_off], ax
    ret

fcb_restore_dta:
    mov ax, word ptr cs:[fcb_saved_dta_seg]
    mov word ptr cs:[dta_seg], ax
    mov ax, word ptr cs:[fcb_saved_dta_off]
    mov word ptr cs:[dta_off], ax
    ret

/*
 * Fill user DTA as FCB from fcb_find_dta hit + search FCB drive at DS:BX.
 */
fcb_result_to_dta:
    push ax
    push cx
    push si
    push di
    push es
    push ds

    mov es, word ptr cs:[fcb_saved_dta_seg]
    mov di, word ptr cs:[fcb_saved_dta_off]
    push di
    mov cx, 37
    xor al, al
    rep stosb
    pop di

    mov al, [bx + FCB_DRV]
    test al, al
    jnz .frtd_drv
    mov al, byte ptr cs:[cur_drive]
    inc al
.frtd_drv:
    mov es:[di], al

    push di
    add di, 1
    mov cx, 11
    mov al, ' '
    rep stosb
    pop di

    push cs
    pop ds
    lea si, [fcb_find_dta + 0x1E]
    mov cx, 8
    mov bx, di
    add bx, 1
.frtd_stem:
    lodsb
    test al, al
    jz .frtd_meta
    cmp al, '.'
    je .frtd_ext
    mov es:[bx], al
    inc bx
    loop .frtd_stem
    lodsb
    cmp al, '.'
    jne .frtd_meta
.frtd_ext:
    mov bx, di
    add bx, 9
    mov cx, 3
.frtd_extc:
    lodsb
    test al, al
    jz .frtd_meta
    mov es:[bx], al
    inc bx
    loop .frtd_extc

.frtd_meta:
    mov ax, word ptr cs:[fcb_find_dta + 0x1A]
    mov es:[di + FCB_FSIZE], ax
    mov ax, word ptr cs:[fcb_find_dta + 0x1C]
    mov es:[di + FCB_FSIZE + 2], ax
    mov ax, word ptr cs:[fcb_find_dta + 0x16]
    mov es:[di + FCB_TIME], ax
    mov ax, word ptr cs:[fcb_find_dta + 0x18]
    mov es:[di + FCB_DATE], ax
    mov word ptr es:[di + FCB_RECSIZ], 0

    pop ds
    pop es
    pop di
    pop si
    pop cx
    pop ax
    ret

/* Drive from FCB DS:BX -> DL; mount. */
fcb_mount_from_fcb:
    mov al, [bx + FCB_DRV]
    test al, al
    jz .fmf_def
    dec al
    jmp .fmf_go
.fmf_def:
    mov al, byte ptr cs:[cur_drive]
.fmf_go:
    mov dl, al
    call dos_mount_drive
    ret

/* AH=11h find first FCB */
dos_ah11:
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

    mov bx, dx
    call fcb_mount_from_fcb
    jc .a11_fail
    mov si, bx
    call fcb_pattern_to_find
    call fcb_find_init_dta
    call fcb_use_find_dta
    push cs
    pop ds
    call find_search
    pushf
    call fcb_restore_dta
    popf
    jc .a11_fail

    pop es
    pop ds
    push ds
    push es
    call fcb_result_to_dta
    call dos_remount_current
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    xor al, al
    clc
    jmp int21_iret

.a11_fail:
    call dos_remount_current
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov al, 0xFF
    clc
    jmp int21_iret

/* AH=12h find next FCB */
dos_ah12:
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

    mov bx, dx
    mov dl, byte ptr cs:[fcb_find_dta + 6]
    call dos_mount_drive
    jc .a12_fail
    call fcb_use_find_dta
    push cs
    pop ds
    call find_search
    pushf
    call fcb_restore_dta
    popf
    jc .a12_fail

    pop es
    pop ds
    push ds
    push es
    call fcb_result_to_dta
    call dos_remount_current
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    xor al, al
    clc
    jmp int21_iret

.a12_fail:
    call dos_remount_current
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov al, 0xFF
    clc
    jmp int21_iret

/*
 * AH=29h parse filename into FCB.
 * AL = parse flags; DS:SI = string; ES:DI = FCB.
 * Out: AL=00 none, 01 wildcards, FF bad drive; SI advanced.
 */
dos_ah29:
    push bx
    push cx
    push dx
    push di
    push es
    push bp

    mov bp, ax
    mov byte ptr cs:[fcb_parse_wild], 0

    test bp, 1
    jz .a29_drv
.a29_skip:
    mov al, [si]
    call fcb_is_sep
    jnc .a29_drv
    inc si
    jmp .a29_skip

.a29_drv:
    mov al, [si]
    cmp al, 'A'
    jb .a29_nodrv
    cmp al, 'Z'
    jbe .a29_drvchk
    cmp al, 'a'
    jb .a29_nodrv
    cmp al, 'z'
    ja .a29_nodrv
.a29_drvchk:
    cmp byte ptr [si + 1], ':'
    jne .a29_nodrv
    call fcb_up
    sub al, 'A'
    cmp al, 26
    jae .a29_baddrv
    inc al
    mov es:[di + FCB_DRV], al
    add si, 2
    jmp .a29_name
.a29_nodrv:
    test bp, 2
    jnz .a29_name
    mov byte ptr es:[di + FCB_DRV], 0
.a29_name:
    test bp, 4
    jnz .a29_ext_keep
    push di
    lea di, [di + FCB_NAME]
    mov cx, 8
    mov al, ' '
    rep stosb
    pop di
.a29_ext_keep:
    test bp, 8
    jnz .a29_parse_name
    push di
    lea di, [di + 9]
    mov cx, 3
    mov al, ' '
    rep stosb
    pop di

.a29_parse_name:
    mov al, [si]
    call fcb_is_sep
    jc .a29_done_ok
    test al, al
    jz .a29_done_ok
    cmp al, 0x0D
    je .a29_done_ok
    cmp al, '.'
    je .a29_dot

    lea bx, [di + FCB_NAME]
    mov cx, 8
.a29_stem:
    mov al, [si]
    test al, al
    jz .a29_done_ok
    cmp al, 0x0D
    je .a29_done_ok
    call fcb_is_sep
    jc .a29_done_ok
    cmp al, '.'
    je .a29_dot
    cmp al, '*'
    je .a29_star_n
    cmp al, '?'
    jne .a29_stem_ch
    mov byte ptr cs:[fcb_parse_wild], 1
.a29_stem_ch:
    call fcb_up
    mov es:[bx], al
    inc bx
    inc si
    loop .a29_stem
.a29_stem_sw:
    mov al, [si]
    test al, al
    jz .a29_done_ok
    call fcb_is_sep
    jc .a29_done_ok
    cmp al, '.'
    je .a29_dot
    inc si
    jmp .a29_stem_sw

.a29_star_n:
    mov byte ptr cs:[fcb_parse_wild], 1
    inc si
.a29_star_nf:
    jcxz .a29_star_n_done
    mov byte ptr es:[bx], '?'
    inc bx
    loop .a29_star_nf
.a29_star_n_done:
    mov al, [si]
    cmp al, '.'
    je .a29_dot
    jmp .a29_done_ok

.a29_dot:
    inc si
    lea bx, [di + 9]
    mov cx, 3
.a29_ext:
    mov al, [si]
    test al, al
    jz .a29_done_ok
    cmp al, 0x0D
    je .a29_done_ok
    call fcb_is_sep
    jc .a29_done_ok
    cmp al, '.'
    je .a29_done_ok
    cmp al, '*'
    je .a29_star_e
    cmp al, '?'
    jne .a29_ext_ch
    mov byte ptr cs:[fcb_parse_wild], 1
.a29_ext_ch:
    call fcb_up
    mov es:[bx], al
    inc bx
    inc si
    loop .a29_ext
    jmp .a29_done_ok

.a29_star_e:
    mov byte ptr cs:[fcb_parse_wild], 1
    inc si
.a29_star_ef:
    jcxz .a29_done_ok
    mov byte ptr es:[bx], '?'
    inc bx
    loop .a29_star_ef

.a29_done_ok:
    mov al, byte ptr cs:[fcb_parse_wild]
    pop bp
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    clc
    jmp int21_iret

.a29_baddrv:
    mov al, 0xFF
    pop bp
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    clc
    jmp int21_iret

/* CF set if AL is a DOS filename separator (or control/space). */
fcb_is_sep:
    cmp al, ' '
    je .fis_yes
    cmp al, 9
    je .fis_yes
    cmp al, ','
    je .fis_yes
    cmp al, ';'
    je .fis_yes
    cmp al, '='
    je .fis_yes
    cmp al, '+'
    je .fis_yes
    cmp al, '/'
    je .fis_yes
    cmp al, '"'
    je .fis_yes
    cmp al, '['
    je .fis_yes
    cmp al, ']'
    je .fis_yes
    cmp al, '<'
    je .fis_yes
    cmp al, '>'
    je .fis_yes
    cmp al, '|'
    je .fis_yes
    cmp al, 0x20
    jb .fis_yes
    clc
    ret
.fis_yes:
    stc
    ret

fcb_up:
    cmp al, 'a'
    jb .fu_ret
    cmp al, 'z'
    ja .fu_ret
    sub al, 0x20
.fu_ret:
    ret
