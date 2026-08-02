/*
 * INT 2Fh — multiplex. Install-check probes return "not installed"
 * cleanly; DOS internal AH=12 AL=0 answers installed.
 */

install_int2f_vector:
    push ax
    push bx
    push es
    xor ax, ax
    mov es, ax
    lea bx, [int2f_handler]
    mov word ptr es:[0x2F * 4], bx
    mov word ptr es:[0x2F * 4 + 2], cs
    pop es
    pop bx
    pop ax
    ret

int2f_handler:
    sti
    cmp ah, 0x12
    je .i2f_12
    cmp ah, 0x01
    je .i2f_not_inst
    cmp ah, 0x10
    je .i2f_not_inst
    cmp ah, 0x43
    je .i2f_not_inst
    cmp ax, 0x1600
    je .i2f_1600
    cmp ah, 0x16
    je .i2f_iret
    cmp ax, 0x1680
    je .i2f_iret
    cmp ah, 0xB7
    je .i2f_append
    iret

.i2f_12:
    /* DOS internal multiplex — AL=0 → installed; AL=E0/E1 rmDOS SUBST */
    cmp al, 0
    je .i2f_12_inst
    cmp al, 0xE0
    je dos_subst_set
    cmp al, 0xE1
    je dos_subst_del
    iret
.i2f_12_inst:
    mov al, 0xFF
    iret

.i2f_1600:
    /* Windows enhanced mode check — return AL=0 (not present) */
    xor al, al
    iret

.i2f_append:
    /* AH=B7 AL=0 APPEND install check → not installed */
    cmp al, 0
    jne .i2f_iret
    /* fall through */
.i2f_not_inst:
    xor al, al
    iret

.i2f_iret:
    iret

/*
 * INT 2Fh AX=12E0h — SUBST set: BL=subst drive (0=A), BH=real drive,
 * DS:DX = ASCIZ path on real drive ("\FOO" or "FOO" or empty for root).
 * INT 2Fh AX=12E1h — SUBST delete: BL=subst drive.
 */
dos_subst_set:
    push ax
    push cx
    push si
    push di
    push es
    push ds
    cmp bl, CDS_COUNT
    jae .dss_bad
    cmp bh, CDS_COUNT
    jae .dss_bad
    mov byte ptr cs:[subst_real + bx], bh
    push cs
    pop es
    mov ax, bx
    mov cl, 6
    shl ax, cl
    lea di, [subst_prefix]
    add di, ax
    mov cx, 64
    xor al, al
    push di
    rep stosb
    pop di
    mov si, dx
    /* skip spaces */
.dss_sp:
    cmp byte ptr [si], ' '
    jne .dss_sp_done
    inc si
    jmp .dss_sp
.dss_sp_done:
    /* skip optional drive letter */
    cmp byte ptr [si + 1], ':'
    jne .dss_path
    add si, 2
.dss_path:
    cmp byte ptr [si], 0
    je .dss_ok
    cmp byte ptr [si], '\\'
    je .dss_copy
    mov byte ptr es:[di], '\\'
    inc di
.dss_copy:
    mov cx, 62
.dss_cp:
    lodsb
    stosb
    test al, al
    jz .dss_ok
    loop .dss_cp
    mov byte ptr es:[di], 0
.dss_ok:
    push cs
    pop ds
    call dos_sysvars_refresh
    pop ds
    pop es
    pop di
    pop si
    pop cx
    pop ax
    mov al, 0
    iret
.dss_bad:
    pop ds
    pop es
    pop di
    pop si
    pop cx
    pop ax
    mov al, 1
    iret

dos_subst_del:
    push ax
    push cx
    push di
    push es
    push ds
    cmp bl, CDS_COUNT
    jae .dsd_bad2
    mov byte ptr cs:[subst_real + bx], 0xFF
    push cs
    pop es
    mov ax, bx
    mov cl, 6
    shl ax, cl
    lea di, [subst_prefix]
    add di, ax
    mov cx, 64
    xor al, al
    rep stosb
    push cs
    pop ds
    call dos_sysvars_refresh
    pop ds
    pop es
    pop di
    pop cx
    pop ax
    xor al, al
    iret
.dsd_bad2:
    pop ds
    pop es
    pop di
    pop cx
    pop ax
    mov al, 1
    iret
