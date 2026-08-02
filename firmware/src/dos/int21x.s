.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * INT21X.COM — gate new INT 21h / INT 2Fh APIs.
 * Prints FCB OK / TEMP OK / PSP OK / IOCTL OK then INT21X OK.
 */

_start:
    push cs
    pop ds

    /* --- FCB create / random block write+read / rename / delete --- */
    mov ah, 0x1A
    lea dx, [dta]
    int 0x21

    lea di, [fcb]
    mov cx, 40
    xor al, al
    rep stosb
    lea di, [fcb + 1]
    mov cx, 11
    mov al, ' '
    rep stosb
    mov byte ptr [fcb], 0
    mov byte ptr [fcb + 1], 'F'
    mov byte ptr [fcb + 2], 'C'
    mov byte ptr [fcb + 3], 'B'
    mov byte ptr [fcb + 4], 'T'
    mov byte ptr [fcb + 5], 'S'
    mov byte ptr [fcb + 6], 'T'
    mov byte ptr [fcb + 9], 'D'
    mov byte ptr [fcb + 10], 'A'
    mov byte ptr [fcb + 11], 'T'
    mov word ptr [fcb + 0x0E], 4

    mov ah, 0x16
    lea dx, [fcb]
    int 0x21
    cmp al, 0
    je .fcb_c1
    jmp fail_fcb
.fcb_c1:

    /* random block write 1 record "RM!!" at relrec 0 */
    mov byte ptr [dta], 'R'
    mov byte ptr [dta + 1], 'M'
    mov byte ptr [dta + 2], '!'
    mov byte ptr [dta + 3], '!'
    mov word ptr [fcb + 0x21], 0
    mov byte ptr [fcb + 0x23], 0
    mov cx, 1
    mov ah, 0x28
    lea dx, [fcb]
    int 0x21
    cmp al, 0
    jne fail_fcb
    cmp cx, 1
    jne fail_fcb

    /* random block read back */
    mov byte ptr [dta], 0
    mov byte ptr [dta + 1], 0
    mov byte ptr [dta + 2], 0
    mov byte ptr [dta + 3], 0
    mov word ptr [fcb + 0x21], 0
    mov byte ptr [fcb + 0x23], 0
    mov cx, 1
    mov ah, 0x27
    lea dx, [fcb]
    int 0x21
    cmp al, 0
    jne fail_fcb
    cmp byte ptr [dta], 'R'
    jne fail_fcb
    cmp byte ptr [dta + 1], 'M'
    jne fail_fcb

    mov ah, 0x10
    lea dx, [fcb]
    int 0x21
    cmp al, 0
    jne fail_fcb

    /* rename FCBTST.DAT -> FCBNEW.DAT (new name at FCB+11h) */
    lea di, [fcb]
    mov cx, 40
    xor al, al
    rep stosb
    lea di, [fcb + 1]
    mov cx, 11
    mov al, ' '
    rep stosb
    mov byte ptr [fcb + 1], 'F'
    mov byte ptr [fcb + 2], 'C'
    mov byte ptr [fcb + 3], 'B'
    mov byte ptr [fcb + 4], 'T'
    mov byte ptr [fcb + 5], 'S'
    mov byte ptr [fcb + 6], 'T'
    mov byte ptr [fcb + 9], 'D'
    mov byte ptr [fcb + 10], 'A'
    mov byte ptr [fcb + 11], 'T'
    lea di, [fcb + 0x11]
    mov cx, 11
    mov al, ' '
    rep stosb
    mov byte ptr [fcb + 0x11], 'F'
    mov byte ptr [fcb + 0x12], 'C'
    mov byte ptr [fcb + 0x13], 'B'
    mov byte ptr [fcb + 0x14], 'N'
    mov byte ptr [fcb + 0x15], 'E'
    mov byte ptr [fcb + 0x16], 'W'
    mov byte ptr [fcb + 0x19], 'D'
    mov byte ptr [fcb + 0x1A], 'A'
    mov byte ptr [fcb + 0x1B], 'T'
    mov ah, 0x17
    lea dx, [fcb]
    int 0x21
    cmp al, 0
    jne fail_fcb

    /* delete FCBNEW.DAT */
    lea di, [fcb]
    mov cx, 40
    xor al, al
    rep stosb
    lea di, [fcb + 1]
    mov cx, 11
    mov al, ' '
    rep stosb
    mov byte ptr [fcb + 1], 'F'
    mov byte ptr [fcb + 2], 'C'
    mov byte ptr [fcb + 3], 'B'
    mov byte ptr [fcb + 4], 'N'
    mov byte ptr [fcb + 5], 'E'
    mov byte ptr [fcb + 6], 'W'
    mov byte ptr [fcb + 9], 'D'
    mov byte ptr [fcb + 10], 'A'
    mov byte ptr [fcb + 11], 'T'
    mov ah, 0x13
    lea dx, [fcb]
    int 0x21
    cmp al, 0
    jne fail_fcb

    /* Extended FCB find: attr 0x10 include directories, pattern *.* */
    lea di, [extfcb]
    mov cx, 48
    xor al, al
    rep stosb
    mov byte ptr [extfcb], 0xFF
    mov byte ptr [extfcb + 6], 0x10
    lea di, [extfcb + 8]
    mov cx, 11
    mov al, '?'
    rep stosb
    mov ah, 0x1A
    lea dx, [dta]
    int 0x21
    mov ah, 0x11
    lea dx, [extfcb]
    int 0x21
    cmp al, 0
    jne fail_fcb
    cmp byte ptr [dta], 0xFF
    jne fail_fcb

    /* AH=65 AL=01 extended country info */
    push es
    push cs
    pop es
    lea di, [country_buf]
    mov ax, 0x6501
    int 0x21
    pop es
    jc fail_fcb
    cmp word ptr [country_buf + 21], 0
    je fail_fcb
    cmp word ptr [country_buf + 23], 0
    je fail_fcb
    mov al, 'q'
    call dword ptr [country_buf + 21]
    cmp al, 'q'
    jne fail_fcb

    mov ah, 0x09
    lea dx, [msg_fcb]
    int 0x21

    /* --- TEMP --- */
    push cs
    pop ds
    xor cx, cx
    lea dx, [plain_tmp]
    mov ah, 0x3C
    int 0x21
    jnc .t_3c_ok
    jmp fail_temp
.t_3c_ok:
    mov [handle], ax
    mov ah, 0x3E
    mov bx, [handle]
    int 0x21
    mov ah, 0x41
    lea dx, [plain_tmp]
    int 0x21

    xor cx, cx
    lea dx, [plain_tmp]
    mov ah, 0x5B
    int 0x21
    jnc .t_5b_ok
    jmp fail_temp
.t_5b_ok:
    mov [handle], ax
    mov ah, 0x3E
    mov bx, [handle]
    int 0x21
    xor cx, cx
    lea dx, [plain_tmp]
    mov ah, 0x5B
    int 0x21
    jc .t_5b_exists
    jmp fail_temp
.t_5b_exists:
    mov ah, 0x41
    lea dx, [plain_tmp]
    int 0x21

    lea di, [tmpbuf]
    mov byte ptr [di], 0
    xor cx, cx
    lea dx, [tmpbuf]
    mov ah, 0x5A
    int 0x21
    jnc .t_5a_ok
    jmp fail_temp
.t_5a_ok:
    mov [handle], ax
    mov ah, 0x3E
    mov bx, [handle]
    int 0x21
    mov ah, 0x41
    lea dx, [tmpbuf]
    int 0x21

    mov ax, 0x5C00
    xor bx, bx
    xor cx, cx
    xor dx, dx
    xor si, si
    int 0x21
    jc .t_5c_ok
    jmp fail_temp
.t_5c_ok:
    cmp ax, 1
    je .t_5c_ax
    jmp fail_temp
.t_5c_ax:

    push cs
    pop ds
    push cs
    pop es
    lea si, [relname]
    lea di, [truebuf]
    mov ah, 0x60
    int 0x21
    jnc .t_60_cf
    jmp fail_temp
.t_60_cf:
    lea si, [truebuf]
    lea di, [true_expect]
.t_60_cmp:
    lodsb
    cmp al, es:[di]
    jne fail_temp
    inc di
    test al, al
    jnz .t_60_cmp
.t_60_ok:

    /* Truename expands a SUBST drive to its real drive and prefix. */
    mov ax, 0x12E0
    mov bx, 0x0004                 /* E: → A: */
    lea dx, [true_subst_path]
    int 0x2F
    test al, al
    jnz fail_temp
    lea si, [true_subst_src]
    lea di, [truebuf]
    mov ah, 0x60
    int 0x21
    jc fail_temp
    lea si, [truebuf]
    lea di, [true_subst_expect]
.t_60_subst_cmp:
    lodsb
    cmp al, es:[di]
    jne fail_temp
    inc di
    test al, al
    jnz .t_60_subst_cmp
    mov ax, 0x12E1
    mov bl, 4
    int 0x2F
    test al, al
    jnz fail_temp

    mov ax, 0x38FF
    mov bx, 1
    int 0x21
    jnc .t_38s
    jmp fail_temp
.t_38s:
    xor al, al
    mov ah, 0x38
    lea dx, [country_buf]
    int 0x21
    jnc .t_38g
    jmp fail_temp
.t_38g:
    cmp bx, 1
    je .t_38_ok
    jmp fail_temp
.t_38_ok:

    mov ah, 0x09
    lea dx, [msg_temp]
    int 0x21

    /* --- PSP 50/51/62 + AH=59 + AH=52 --- */
    mov ah, 0x51
    int 0x21
    mov [saved_psp], bx
    mov ah, 0x50
    int 0x21
    mov ah, 0x62
    int 0x21
    cmp bx, word ptr [saved_psp]
    je .psp_c1
    jmp fail_psp
.psp_c1:
    mov es, bx
    cmp word ptr es:[0x32], 20
    jne fail_psp
    cmp word ptr es:[0x34], 0x18
    jne fail_psp
    cmp word ptr es:[0x36], bx
    jne fail_psp
    cmp byte ptr es:[0x18], 0x01
    jne fail_psp
    cmp byte ptr es:[0x19], 0x01
    jne fail_psp
    cmp byte ptr es:[0x1A], 0x01
    jne fail_psp

    mov ax, 0x3D00
    lea dx, [nosuch]
    int 0x21
    jc .psp_c3
    jmp fail_psp
.psp_c3:
    mov ah, 0x59
    xor bx, bx
    int 0x21
    cmp ax, 2
    je .psp_c4
    jmp fail_psp
.psp_c4:

    mov ah, 0x52
    int 0x21
    mov ax, es
    test ax, ax
    jnz .psp_c5
    jmp fail_psp
.psp_c5:
    mov ax, es:[bx]
    test ax, ax
    jnz .psp_c6
    jmp fail_psp
.psp_c6:
    /* SFT pointer at +04 and CDS at +16 must be nonzero offs */
    mov ax, es:[bx + 4]
    test ax, ax
    jnz .psp_c7
    jmp fail_psp
.psp_c7:
    mov ax, es:[bx + 0x16]
    test ax, ax
    jnz .psp_c8
    jmp fail_psp
.psp_c8:
    mov ah, 0x19
    int 0x21
    cmp al, byte ptr es:[bx + 0x22]
    je .psp_c9
    jmp fail_psp
.psp_c9:
    /* Live CDS: MD+CD then AH=52 path for A: must contain marker */
    mov ah, 0x39
    lea dx, [cds_dir]
    int 0x21
    jc .psp_cds_skip
    mov ah, 0x3B
    lea dx, [cds_dir]
    int 0x21
    jc .psp_cds_skip
    mov ah, 0x52
    int 0x21
    mov si, es:[bx + 0x16]
    mov ax, es:[bx + 0x18]
    mov es, ax
    /* CDS[0] path bytes — look for 'C','D','S','P' */
    mov di, si
    add di, 2
    mov cx, 64
.psp_cds_scan:
    cmp byte ptr es:[di], 'C'
    jne .psp_cds_n
    cmp byte ptr es:[di + 1], 'D'
    jne .psp_cds_n
    cmp byte ptr es:[di + 2], 'S'
    jne .psp_cds_n
    cmp byte ptr es:[di + 3], 'P'
    je .psp_cds_ok
.psp_cds_n:
    inc di
    loop .psp_cds_scan
    jmp fail_psp
.psp_cds_ok:
    mov ah, 0x3B
    lea dx, [cds_root]
    int 0x21
.psp_cds_skip:

    mov ah, 0x09
    lea dx, [msg_psp]
    int 0x21

    /* --- IOCTL 06 + INT 2F SHARE/APPEND --- */
    mov ax, 0x4406
    xor bx, bx
    int 0x21
    jnc .t_ioctl_cf
    jmp fail_ioctl
.t_ioctl_cf:
    cmp al, 0xFF
    je .t_ioctl_al
    jmp fail_ioctl
.t_ioctl_al:

    /* AL=03 char-device control write to CON (handle 1) */
    mov ax, 0x4403
    mov bx, 1
    mov cx, 1
    lea dx, [ioctl_ch]
    int 0x21
    jnc .t_ioctl03
    jmp fail_ioctl
.t_ioctl03:
    cmp ax, 1
    je .t_ioctl03b
    jmp fail_ioctl
.t_ioctl03b:

    /* AL=04/05 control channels are unsupported: CF + AX=1. */
    mov ax, 0x4404
    mov bx, 1
    int 0x21
    jc .t_ioctl04
    jmp fail_ioctl
.t_ioctl04:
    cmp ax, 1
    je .t_ioctl04z
    jmp fail_ioctl
.t_ioctl04z:
    mov ax, 0x4405
    mov bx, 1
    int 0x21
    jc .t_ioctl05
    jmp fail_ioctl
.t_ioctl05:
    cmp ax, 1
    je .t_ioctl05z
    jmp fail_ioctl
.t_ioctl05z:

    /* AL=0Dh generic block IOCTL is also unsupported. */
    mov ax, 0x440D
    xor bx, bx
    int 0x21
    jc .t_ioctl0d
    jmp fail_ioctl
.t_ioctl0d:
    cmp ax, 1
    je .t_ioctl0dz
    jmp fail_ioctl
.t_ioctl0dz:

    /* AL=09 reports a local drive with AX=0. */
    mov ax, 0x4409
    mov bl, 1
    int 0x21
    jc fail_ioctl
    test ax, ax
    jnz fail_ioctl

    /* AL=02 char read from NUL */
    mov ax, 0x3D00
    lea dx, [nul_name]
    int 0x21
    jnc .t_nul
    jmp fail_ioctl
.t_nul:
    mov bx, ax
    mov ax, 0x4402
    mov cx, 1
    lea dx, [ioctl_ch]
    int 0x21
    pushf
    mov ah, 0x3E
    int 0x21
    popf
    jnc .t_ioctl02
    jmp fail_ioctl
.t_ioctl02:

    mov ax, 0x1000
    int 0x2F
    test al, al
    jz .t_share
    jmp fail_ioctl
.t_share:

    mov ax, 0xB700
    int 0x2F
    test al, al
    jz .t_append
    jmp fail_ioctl
.t_append:

    mov ah, 0x09
    lea dx, [msg_ioctl]
    int 0x21

    /* --- AUX/PRN / verify / InDOS / DPB / alloc strat / commit --- */
    mov ah, 0x03
    int 0x21
    mov ah, 0x04
    mov dl, 'X'
    int 0x21
    mov ah, 0x05
    mov dl, 'Y'
    int 0x21

    mov ah, 0x2E
    mov al, 1
    int 0x21
    mov ah, 0x54
    int 0x21
    cmp al, 1
    je .x_ver
    mov ah, 0x09
    lea dx, [msg_xf1]
    int 0x21
    jmp fail_exit
.x_ver:
    mov ah, 0x2E
    xor al, al
    int 0x21

    mov ah, 0x34
    int 0x21
    mov ax, es
    test ax, ax
    jnz .x_indos
    mov ah, 0x09
    lea dx, [msg_xf2]
    int 0x21
    jmp fail_exit
.x_indos:
    cmp byte ptr es:[bx], 0
    je .x_indos2
    mov ah, 0x09
    lea dx, [msg_xf2]
    int 0x21
    jmp fail_exit
.x_indos2:

    push ds
    mov ah, 0x32
    xor dl, dl
    int 0x21
    cmp al, 0xFF
    jne .x_dpb
    pop ds
    mov ah, 0x09
    lea dx, [msg_xf3]
    int 0x21
    jmp fail_exit
.x_dpb:
    mov ax, ds
    test ax, ax
    jnz .x_dpb2
    pop ds
    mov ah, 0x09
    lea dx, [msg_xf3]
    int 0x21
    jmp fail_exit
.x_dpb2:
    /* Device header far ptr at DPB +13/+15 must be nonzero */
    cmp word ptr ds:[bx + 0x13], 0
    jne .x_dpb_off
    pop ds
    mov ah, 0x09
    lea dx, [msg_xf3]
    int 0x21
    jmp fail_exit
.x_dpb_off:
    cmp word ptr ds:[bx + 0x15], 0
    jne .x_dpb_seg
    pop ds
    mov ah, 0x09
    lea dx, [msg_xf3]
    int 0x21
    jmp fail_exit
.x_dpb_seg:
    mov word ptr cs:[dpb_seen_off], bx
    mov ax, ds
    mov word ptr cs:[dpb_seen_seg], ax
    pop ds

    /* CDS[0]+45 must reference the same per-drive slot as AH=32/AH=1F. */
    mov ah, 0x52
    int 0x21
    mov si, es:[bx + 0x16]
    mov ax, es:[si + 0x45]
    cmp ax, word ptr cs:[dpb_seen_off]
    jne .x_dpb_match_fail
    mov ax, es:[si + 0x47]
    cmp ax, word ptr cs:[dpb_seen_seg]
    jne .x_dpb_match_fail
    push ds
    mov ah, 0x1F
    int 0x21
    cmp al, 0xFF
    je .x_dpb_1f_fail
    cmp bx, word ptr cs:[dpb_seen_off]
    jne .x_dpb_1f_fail
    mov ax, ds
    cmp ax, word ptr cs:[dpb_seen_seg]
    jne .x_dpb_1f_fail
    pop ds
    jmp .x_dpb_match_ok
.x_dpb_1f_fail:
    pop ds
.x_dpb_match_fail:
    mov ah, 0x09
    lea dx, [msg_xf3]
    int 0x21
    jmp fail_exit
.x_dpb_match_ok:

    mov ax, 0x5801
    mov bx, 1
    int 0x21
    jnc .x_s1
    mov ah, 0x09
    lea dx, [msg_xf4]
    int 0x21
    jmp fail_exit
.x_s1:
    mov ax, 0x5800
    int 0x21
    jnc .x_s2
    mov ah, 0x09
    lea dx, [msg_xf4]
    int 0x21
    jmp fail_exit
.x_s2:
    cmp bx, 1
    je .x_strat
    mov ah, 0x09
    lea dx, [msg_xf4]
    int 0x21
    jmp fail_exit
.x_strat:
    /* best-fit alloc while strategy=1 */
    mov bx, 1
    mov ah, 0x48
    int 0x21
    jc .x_bf_fail
    mov es, ax
    mov ah, 0x49
    int 0x21
    jmp .x_bf_ok
.x_bf_fail:
    mov ah, 0x09
    lea dx, [msg_xf4]
    int 0x21
    jmp fail_exit
.x_bf_ok:
    /* last-fit alloc while strategy=2 */
    mov ax, 0x5801
    mov bx, 2
    int 0x21
    jnc .x_lf1
    mov ah, 0x09
    lea dx, [msg_xf4]
    int 0x21
    jmp fail_exit
.x_lf1:
    mov bx, 1
    mov ah, 0x48
    int 0x21
    jc .x_lf_fail
    mov es, ax
    mov ah, 0x49
    int 0x21
    jmp .x_lf_ok
.x_lf_fail:
    mov ah, 0x09
    lea dx, [msg_xf4]
    int 0x21
    jmp fail_exit
.x_lf_ok:
    mov ax, 0x5801
    xor bx, bx
    int 0x21

    mov ax, 0x3C00
    lea dx, [xtra_name]
    xor cx, cx
    int 0x21
    jnc .x_cr
    mov ah, 0x09
    lea dx, [msg_xf5]
    int 0x21
    jmp fail_exit
.x_cr:
    mov bx, ax
    mov word ptr [xtra_h], ax
    mov ah, 0x68
    int 0x21
    jnc .x_cm
    mov ah, 0x09
    lea dx, [msg_xf5]
    int 0x21
    jmp fail_exit
.x_cm:
    /* AH=57 get/set file datetime */
    mov bx, word ptr [xtra_h]
    mov ax, 0x5700
    int 0x21
    jnc .x_dt
    mov ah, 0x09
    lea dx, [msg_xf5]
    int 0x21
    jmp fail_exit
.x_dt:
    mov bx, word ptr [xtra_h]
    mov ax, 0x5701
    int 0x21
    jnc .x_dt2
    mov ah, 0x09
    lea dx, [msg_xf5]
    int 0x21
    jmp fail_exit
.x_dt2:
    /* AH=45 dup then AH=46 force-dup onto the dup handle */
    mov bx, word ptr [xtra_h]
    mov ah, 0x45
    int 0x21
    jnc .x_dup
    mov ah, 0x09
    lea dx, [msg_xf5]
    int 0x21
    jmp fail_exit
.x_dup:
    mov cx, ax
    mov bx, word ptr [xtra_h]
    mov ah, 0x46
    int 0x21
    jnc .x_fdup
    mov ah, 0x3E
    mov bx, cx
    int 0x21
    mov ah, 0x09
    lea dx, [msg_xf5]
    int 0x21
    jmp fail_exit
.x_fdup:
    mov ah, 0x3E
    mov bx, cx
    int 0x21
    mov ah, 0x3E
    mov bx, word ptr [xtra_h]
    int 0x21

    /* ES may still be kernel from AH=34 — restore before stosb */
    push cs
    pop es

    /* FCB AH=24 set relative record (no disk) */
    lea di, [fcb]
    mov cx, 40
    xor al, al
    rep stosb
    mov word ptr [fcb + 0x0C], 1
    mov byte ptr [fcb + 0x20], 2
    mov ah, 0x24
    lea dx, [fcb]
    int 0x21
    cmp word ptr [fcb + 0x21], 130
    je .x_rel
    mov ah, 0x09
    lea dx, [msg_xf7]
    int 0x21
    jmp fail_exit
.x_rel:

    /* FCB AH=23/26: create SZ.DAT, size, create-new conflict */
    mov ah, 0x1A
    lea dx, [dta]
    int 0x21
    push cs
    pop es
    lea di, [fcb]
    mov cx, 40
    xor al, al
    rep stosb
    lea di, [fcb + 1]
    mov cx, 11
    mov al, ' '
    rep stosb
    mov byte ptr [fcb + 1], 'S'
    mov byte ptr [fcb + 2], 'Z'
    mov byte ptr [fcb + 9], 'D'
    mov byte ptr [fcb + 10], 'A'
    mov byte ptr [fcb + 11], 'T'
    mov word ptr [fcb + 0x0E], 1
    mov ah, 0x16
    lea dx, [fcb]
    int 0x21
    cmp al, 0
    je .x_mk
    mov ah, 0x09
    lea dx, [msg_xf6]
    int 0x21
    jmp fail_exit
.x_mk:
    mov bl, [fcb + 0x18]
    xor bh, bh
    mov ah, 0x40
    mov cx, 4
    lea dx, [dta]
    mov byte ptr [dta], 'S'
    mov byte ptr [dta+1], 'I'
    mov byte ptr [dta+2], 'Z'
    mov byte ptr [dta+3], 'E'
    int 0x21
    mov ah, 0x10
    lea dx, [fcb]
    int 0x21
    mov ah, 0x23
    lea dx, [fcb]
    int 0x21
    cmp al, 0
    jne .x_szf
    cmp word ptr [fcb + 0x21], 4
    je .x_sz2
.x_szf:
    mov ah, 0x09
    lea dx, [msg_xf6]
    int 0x21
    jmp fail_exit
.x_sz2:
    /* create-new via AH=5B (was wrongly gated on AH=26) */
    mov ah, 0x5B
    xor cx, cx
    lea dx, [newfcb_name]
    int 0x21
    jnc .x_new
    mov ah, 0x09
    lea dx, [msg_xf8]
    int 0x21
    jmp fail_exit
.x_new:
    mov bx, ax
    mov ah, 0x3E
    int 0x21

    /* AH=2F get DTA after AH=1A */
    mov ah, 0x1A
    lea dx, [dta]
    int 0x21
    mov ah, 0x2F
    int 0x21
    mov ax, es
    mov cx, cs
    cmp ax, cx
    jne .x_dtaf
    lea ax, [dta]
    cmp bx, ax
    je .x_dta_ok
.x_dtaf:
    mov ah, 0x09
    lea dx, [msg_xf8]
    int 0x21
    jmp fail_exit
.x_dta_ok:

    /* AH=1B allocation info (sets DS:BX → media; restore DS after) */
    push ds
    mov ah, 0x1B
    int 0x21
    test al, al
    jz .x_1bf
    cmp cx, 512
    jne .x_1bf
    test dx, dx
    jz .x_1bf
    pop ds
    jmp .x_1b_ok
.x_1bf:
    pop ds
    mov ah, 0x09
    lea dx, [msg_xf9]
    int 0x21
    jmp fail_exit
.x_1b_ok:

    /* AH=1F default DPB + AH=1C for A: (match 1B spc) */
    push ds
    mov ah, 0x1F
    int 0x21
    cmp al, 0xFF
    je .x_1ff
    mov ax, ds
    test ax, ax
    jz .x_1ff
    cmp byte ptr ds:[bx], 0          /* drive A = 0 */
    jne .x_1ff
    pop ds
    jmp .x_1f_ok
.x_1ff:
    pop ds
    mov ah, 0x09
    lea dx, [msg_xfa]
    int 0x21
    jmp fail_exit
.x_1f_ok:

    push ds
    mov ah, 0x1B
    int 0x21
    xor ah, ah
    mov si, ax                       /* SI = default spc (BX clobbered by 1C) */
    pop ds
    push ds
    mov ah, 0x1C
    mov dl, 1                        /* A: */
    int 0x21
    xor ah, ah
    cmp ax, si
    jne .x_1cf
    cmp cx, 512
    jne .x_1cf
    test dx, dx
    jz .x_1cf
    pop ds
    jmp .x_1c_ok
.x_1cf:
    pop ds
    mov ah, 0x09
    lea dx, [msg_xfb]
    int 0x21
    jmp fail_exit
.x_1c_ok:

    /* AH=0Eh returns LASTDRIVE while selecting the requested current drive. */
    mov ah, 0x19
    int 0x21
    mov dl, al
    mov ah, 0x0E
    int 0x21
    cmp al, 8
    jne .x_drive_api_fail

    /* AH=2Dh soft time round-trips through AH=2Ch without RTC hardware. */
    mov ch, 12
    mov cl, 34
    mov dh, 56
    mov dl, 78
    mov ah, 0x2D
    int 0x21
    test al, al
    jnz .x_drive_api_fail
    mov ah, 0x2C
    int 0x21
    cmp ch, 12
    jne .x_drive_api_fail
    cmp cl, 34
    jne .x_drive_api_fail
    cmp dh, 56
    jne .x_drive_api_fail
    cmp dl, 78
    jne .x_drive_api_fail

    /* AH=36 honors DL=1 (A:) and rejects an unmapped requested drive. */
    mov ah, 0x36
    mov dl, 1
    int 0x21
    cmp ax, 0xFFFF
    je .x_drive_api_fail
    test ax, ax
    jz .x_drive_api_fail
    test bx, bx
    jz .x_drive_api_fail
    cmp cx, 512
    jne .x_drive_api_fail
    test dx, dx
    jz .x_drive_api_fail
    mov ah, 0x36
    mov dl, 0x7F
    int 0x21
    cmp ax, 0xFFFF
    je .x_drive_api_ok
.x_drive_api_fail:
    mov ah, 0x09
    lea dx, [msg_xfb]
    int 0x21
    jmp fail_exit
.x_drive_api_ok:

    /* INT 25h absolute read boot sector — expect 55 AA */
    push ds
    push cs
    pop ds
    lea bx, [absbuf]
    mov al, 0
    mov cx, 1
    xor dx, dx
    int 0x25
    pop ax
    sti
    jc .x_25f
    cmp word ptr [absbuf + 510], 0xAA55
    jne .x_25f

    /* DOS 3.31 packet form: DWORD sector, WORD count, far transfer buffer. */
    lea ax, [absbuf]
    mov word ptr [abs_packet + 6], ax
    mov ax, cs
    mov word ptr [abs_packet + 8], ax
    mov word ptr [absbuf + 510], 0
    lea bx, [abs_packet]
    mov al, 0
    mov cx, 0xFFFF
    xor dx, dx
    int 0x25
    pop ax
    sti
    jc .x_25f
    cmp word ptr [absbuf + 510], 0xAA55
    jne .x_25f
    pop ds
    jmp .x_25_ok
.x_25f:
    pop ds
    mov ah, 0x09
    lea dx, [msg_xfc]
    int 0x21
    jmp fail_exit
.x_25_ok:

    /* AH=26 Create New PSP */
    mov ah, 0x51
    int 0x21
    mov word ptr [saved_psp], bx
    mov bx, 0x10
    mov ah, 0x48
    int 0x21
    jc .x_pspf
    mov dx, ax
    mov word ptr [new_psp], ax
    mov ah, 0x26
    int 0x21
    mov ah, 0x51
    int 0x21
    cmp bx, word ptr [new_psp]
    jne .x_pspf
    mov es, bx
    cmp byte ptr es:[0], 0xCD
    jne .x_pspf
    cmp word ptr es:[0x32], 20
    jne .x_pspf
    cmp word ptr es:[0x34], 0x18
    jne .x_pspf
    cmp word ptr es:[0x36], bx
    jne .x_pspf
    cmp byte ptr es:[0x18], 0x01
    jne .x_pspf
    /* restore current PSP */
    mov bx, word ptr [saved_psp]
    mov ah, 0x50
    int 0x21
    /* free child arena */
    mov es, word ptr [new_psp]
    mov ah, 0x49
    int 0x21
    jmp .x_psp_ok
.x_pspf:
    mov bx, word ptr [saved_psp]
    mov ah, 0x50
    int 0x21
    mov ah, 0x09
    lea dx, [msg_xf8]
    int 0x21
    jmp fail_exit
.x_psp_ok:

    /* AH=67 get/set handle count */
    mov ax, 0x6700
    int 0x21
    cmp bx, 20
    jb .x_67f
    mov bx, 40
    mov ax, 0x6701
    int 0x21
    jc .x_67f
    mov ax, 0x6700
    int 0x21
    cmp bx, 40
    jne .x_67f
    mov ah, 0x62
    int 0x21
    mov dx, bx
    mov es, bx
    cmp word ptr es:[0x32], 40
    jne .x_67f
    mov di, word ptr es:[0x34]
    mov ax, word ptr es:[0x36]
    test ax, ax
    jz .x_67f
    cmp ax, dx
    jne .x_67_ptr
    cmp di, 0x18
    je .x_67f
.x_67_ptr:
    mov es, ax
    cmp byte ptr es:[di], 0x01
    jne .x_67f
    cmp byte ptr es:[di + 1], 0x01
    jne .x_67f
    cmp byte ptr es:[di + 2], 0x01
    jne .x_67f
    mov bx, 20
    mov ax, 0x6701
    int 0x21
    mov ah, 0x09
    lea dx, [msg_files]
    int 0x21
    jmp .x_exec1
.x_67f:
    mov ah, 0x09
    lea dx, [msg_xf8]
    int 0x21
    jmp fail_exit
.x_exec1:
    /* AH=4B AL=1 load-only: fill EPB +0E..+14, do not run */
    lea bx, [exec1_epb]
    lea ax, [exec1_tail]
    mov word ptr [bx + 2], ax
    mov word ptr [bx + 4], cs
    lea ax, [exec1_fcb]
    mov word ptr [bx + 6], ax
    mov word ptr [bx + 8], cs
    mov word ptr [bx + 0x0A], ax
    mov word ptr [bx + 0x0C], cs
    mov word ptr [bx + 0x0E], 0
    mov word ptr [bx + 0x10], 0
    mov word ptr [bx + 0x12], 0
    mov word ptr [bx + 0x14], 0
    push cs
    pop es
    lea dx, [exec1_path]
    mov ax, 0x4B01
    int 0x21
    push cs
    pop ds
    jc .x_e1f
    cmp word ptr [exec1_epb + 0x12], 0x0100
    jne .x_e1f
    cmp word ptr [exec1_epb + 0x14], 0
    je .x_e1f
    /* free loaded image */
    mov es, word ptr [exec1_epb + 0x14]
    mov ah, 0x49
    int 0x21
    push cs
    pop ds
    mov ah, 0x09
    lea dx, [msg_exec1]
    int 0x21
    jmp .x_55_start
.x_e1f:
    push cs
    pop ds
    mov ah, 0x09
    lea dx, [msg_xf8]
    int 0x21
    jmp fail_exit
.x_55_start:

    /* AH=55 Create Child PSP — current unchanged */
    mov ah, 0x51
    int 0x21
    mov word ptr [saved_psp], bx
    mov bx, 0x10
    mov ah, 0x48
    int 0x21
    jc .x_55f
    mov dx, ax
    mov word ptr [new_psp], ax
    mov ah, 0x55
    int 0x21
    mov ah, 0x51
    int 0x21
    cmp bx, word ptr [saved_psp]
    jne .x_55f
    mov es, word ptr [new_psp]
    mov ah, 0x49
    int 0x21
    jmp .x_done
.x_55f:
    mov ah, 0x09
    lea dx, [msg_xf8]
    int 0x21
    jmp fail_exit
.x_done:

    /* AUX/PRN: AH=04/05 + handle 4 write + AH=3C create PRN (COPY path) */
    push cs
    pop ds
    mov ah, 0x04
    mov dl, 'A'
    int 0x21
    mov ah, 0x05
    mov dl, 'P'
    int 0x21
    mov bx, 4
    mov ah, 0x40
    mov cx, 1
    lea dx, [auxprn_ch]
    int 0x21
    jc .x_apf
    cmp ax, 1
    jne .x_apf
    /* CREATE "PRN" must open the device (not a disk file named PRN) */
    mov ah, 0x3C
    xor cx, cx
    lea dx, [prn_name]
    int 0x21
    jc .x_apf
    mov bx, ax
    mov ah, 0x40
    mov cx, 1
    lea dx, [auxprn_ch]
    int 0x21
    jc .x_apf
    cmp ax, 1
    jne .x_apf
    mov ah, 0x3E
    int 0x21
    jc .x_apf
    mov ah, 0x09
    lea dx, [msg_auxprn]
    int 0x21
    jmp .x_ap_ok
.x_apf:
    mov ah, 0x09
    lea dx, [msg_xf8]
    int 0x21
    jmp fail_exit
.x_ap_ok:

    /* Ctrl-Break → INT 23h when BREAK ON (k8086 scan inject 0x8901) */
    push ds
    push es
    xor ax, ax
    mov es, ax
    mov ax, es:[0x23 * 4]
    mov word ptr [saved_i23_off], ax
    mov ax, es:[0x23 * 4 + 2]
    mov word ptr [saved_i23_seg], ax
    lea ax, [brk23_isr]
    mov word ptr es:[0x23 * 4], ax
    mov word ptr es:[0x23 * 4 + 2], cs
    mov byte ptr [brk23_flag], 0

    mov ax, 0x3301
    mov dl, 1
    int 0x21

    mov ax, 0x40
    mov ds, ax
    or byte ptr [0x17], 0x04
    xor ax, ax
    mov ds, ax
    mov al, 0x46
    mov dx, 0x8901
    out dx, al
    hlt

    push cs
    pop ds
    cmp byte ptr [brk23_flag], 1
    jne .x_brk_fail

    /* restore INT 23 */
    xor ax, ax
    mov es, ax
    mov ax, word ptr [saved_i23_off]
    mov word ptr es:[0x23 * 4], ax
    mov ax, word ptr [saved_i23_seg]
    mov word ptr es:[0x23 * 4 + 2], ax
    pop es
    pop ds
    mov ah, 0x09
    lea dx, [msg_brk23]
    int 0x21
    jmp .x_brk_ok
.x_brk_fail:
    xor ax, ax
    mov es, ax
    mov ax, word ptr [saved_i23_off]
    mov word ptr es:[0x23 * 4], ax
    mov ax, word ptr [saved_i23_seg]
    mov word ptr es:[0x23 * 4 + 2], ax
    pop es
    pop ds
    mov ah, 0x09
    lea dx, [msg_xf8]
    int 0x21
    jmp fail_exit
.x_brk_ok:

    push cs
    pop ds

    /* AH=5Ch must fail honestly without SHARE (CF, AX=1) */
    mov ax, 0x5C00
    xor bx, bx
    xor cx, cx
    xor dx, dx
    int 0x21
    jnc .x_stub_fail
    cmp ax, 1
    jne .x_stub_fail

    /* AH=5Dh/5Eh/5Fh networking/server surfaces are not installed. */
    mov ax, 0x5D00
    int 0x21
    jnc .x_stub_fail
    cmp ax, 1
    jne .x_stub_fail
    mov ax, 0x5E00
    int 0x21
    jnc .x_stub_fail
    cmp ax, 1
    jne .x_stub_fail
    mov ax, 0x5F00
    int 0x21
    jnc .x_stub_fail
    cmp ax, 1
    jne .x_stub_fail

    /* AH=65h only supports AL=01 extended country information. */
    mov ax, 0x6502
    int 0x21
    jnc .x_stub_fail
    cmp ax, 1
    jne .x_stub_fail

    /* VERIFY flag-only: set ON then get */
    mov ax, 0x2E01
    int 0x21
    mov ah, 0x54
    int 0x21
    cmp al, 1
    jne .x_stub_fail
    mov ax, 0x2E00
    int 0x21

    /* AH=66h unsupported — CF or unchanged AX path */
    mov ax, 0x6601
    int 0x21
    jnc .x_stub_fail

    /* BUFFERS= is parsed, but rmDOS exposes no DOS buffer-chain pointer. */
    mov ah, 0x52
    int 0x21
    cmp word ptr es:[bx + 0x12], 0
    jne .x_stub_fail
    cmp word ptr es:[bx + 0x14], 0
    jne .x_stub_fail
    /* LoL LASTDRIVE byte (default images keep 8) */
    mov al, es:[bx + 0x21]
    cmp al, 8
    jb .x_stub_fail

    mov ah, 0x09
    lea dx, [msg_stub]
    int 0x21

    mov ah, 0x09
    lea dx, [msg_xtra]
    int 0x21

    mov ah, 0x09
    lea dx, [msg_ok]
    int 0x21
    mov ax, 0x4C00
    int 0x21

.x_stub_fail:
    mov ah, 0x09
    lea dx, [msg_stub_fail]
    int 0x21
    jmp fail_exit

fail_fcb:
    mov ah, 0x09
    lea dx, [msg_fcb_fail]
    int 0x21
    jmp fail_exit
fail_psp:
    mov ah, 0x09
    lea dx, [msg_psp_fail]
    int 0x21
    jmp fail_exit
fail_temp:
    mov ah, 0x09
    lea dx, [msg_temp_fail]
    int 0x21
    jmp fail_exit
fail_ioctl:
    mov ah, 0x09
    lea dx, [msg_ioctl_fail]
    int 0x21
    jmp fail_exit
fail_xtra:
    mov ah, 0x09
    lea dx, [msg_xtra_fail]
    int 0x21
fail_exit:
    mov ax, 0x4C01
    int 0x21

msg_fcb:
    .ascii "FCB OK\r\n$"
msg_psp:
    .ascii "PSP OK\r\n$"
msg_temp:
    .ascii "TEMP OK\r\n$"
cds_dir:
    .asciz "CDSP"
cds_root:
    .asciz "\\"
msg_ioctl:
    .ascii "IOCTL OK\r\n$"
msg_xtra:
    .ascii "XTRA OK\r\n$"
msg_stub:
    .ascii "STUB OK\r\n$"
msg_files:
    .ascii "FILES OK\r\n$"
msg_exec1:
    .ascii "EXEC1 OK\r\n$"
msg_auxprn:
    .ascii "AUXPRN OK\r\n$"
msg_brk23:
    .ascii "BREAK23 OK\r\n$"
msg_ok:
    .ascii "INT21X OK\r\n$"
msg_fcb_fail:
    .ascii "FCB FAIL\r\n$"
msg_psp_fail:
    .ascii "PSP FAIL\r\n$"
msg_temp_fail:
    .ascii "TEMP FAIL\r\n$"
msg_ioctl_fail:
    .ascii "IOCTL FAIL\r\n$"
msg_xtra_fail:
    .ascii "XTRA FAIL\r\n$"
msg_stub_fail:
    .ascii "STUB FAIL\r\n$"
msg_xf1:
    .ascii "XF1\r\n$"
msg_xf2:
    .ascii "XF2\r\n$"
msg_xf3:
    .ascii "XF3\r\n$"
msg_xf4:
    .ascii "XF4\r\n$"
msg_xf5:
    .ascii "XF5\r\n$"
msg_xf6:
    .ascii "XF6\r\n$"
msg_xf7:
    .ascii "XF7\r\n$"
msg_xf8:
    .ascii "XF8\r\n$"
msg_xf9:
    .ascii "XF9\r\n$"
msg_xfa:
    .ascii "XFA\r\n$"
msg_xfb:
    .ascii "XFB\r\n$"
msg_xfc:
    .ascii "XFC\r\n$"
nosuch:
    .asciz "NOSUCH.XYZ"
xtra_name:
    .asciz "XTRA.TMP"
nul_name:
    .asciz "NUL"
xtra_h:
    .word 0
dpb_seen_off:
    .word 0
dpb_seen_seg:
    .word 0
relname:
    .asciz "A:\\TEST\\.\\FOO\\..\\SAMPLE.TXT"
true_expect:
    .asciz "A:\\TEST\\SAMPLE.TXT"
true_subst_path:
    .asciz "\\TEST"
true_subst_src:
    .asciz "E:\\.\\SAMPLE.TXT"
true_subst_expect:
    .asciz "A:\\TEST\\SAMPLE.TXT"
plain_tmp:
    .asciz "PLAIN.TMP"
saved_psp:
    .word 0
new_psp:
    .word 0
handle:
    .word 0
newfcb_name:
    .asciz "NEWFCB.DAT"
fcb:
    .space 64, 0
extfcb:
    .space 48, 0
dta:
    .space 128, 0
tmpbuf:
    .space 64, 0
truebuf:
    .space 64, 0
country_buf:
    .space 40, 0
absbuf:
    .space 512, 0
abs_packet:
    .long 0                      /* starting sector */
    .word 1                      /* sector count */
    .word 0, 0                   /* transfer offset, segment */
exec1_path:
    .asciz "A:\\BIN\\MORE.COM"
exec1_tail:
    .byte 0
    .byte 0x0D
exec1_fcb:
    .space 16, 0
exec1_epb:
    .word 0                      /* env */
    .word 0, 0                   /* tail */
    .word 0, 0                   /* fcb1 */
    .word 0, 0                   /* fcb2 */
    .word 0, 0, 0, 0             /* SP SS IP CS */
auxprn_ch:
    .byte '!'
prn_name:
    .asciz "PRN"
ioctl_ch:
    .byte '.'
brk23_flag:
    .byte 0
saved_i23_off:
    .word 0
saved_i23_seg:
    .word 0

brk23_isr:
    push ds
    push ax
    push cs
    pop ds
    mov byte ptr [brk23_flag], 1
    pop ax
    pop ds
    iret
