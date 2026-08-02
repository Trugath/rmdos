/*
 * Path resolver + directory INT 21h handlers (AH=39/3A/3B/43/47/56).
 *
 * Expected CS globals (defined in kernel.s):
 *   cwd_cluster          word  — current dir cluster (0 = root)
 *   cwd_path             64-byte ASCIZ path without drive ("" or "\FOO\BAR")
 *   path_resolve_cluster word  — last path_resolve parent dir cluster
 *   last_size_hi         word  — high word of file size for fat12_update_dirent
 *   cur_drive            byte  — current drive (0 = A:)
 */

/*
 * Convert one path component at DS:SI to CS:name83.
 * Stops at '\', '/', or 0. Advances SI past component and separator.
 * OUT: AL=0 normal, AL=1 ".", AL=2 ".."; CF set if bad name.
 */
path_to_83_component:
    push bx
    push cx
    push di
    push es

    push cs
    pop es
    lea di, [name83]
    mov cx, 11
    mov al, ' '
    rep stosb

    mov al, [si]
    test al, al
    jz .p83c_bad
    cmp al, '\\'
    je .p83c_bad
    cmp al, '/'
    je .p83c_bad

    cmp al, '.'
    jne .p83c_normal
    mov al, [si + 1]
    test al, al
    jz .p83c_dot
    cmp al, '\\'
    je .p83c_dot
    cmp al, '/'
    je .p83c_dot
    cmp al, '.'
    jne .p83c_normal
    mov al, [si + 2]
    test al, al
    jz .p83c_dotdot
    cmp al, '\\'
    je .p83c_dotdot
    cmp al, '/'
    je .p83c_dotdot
    jmp .p83c_normal

.p83c_dot:
    lea di, [name83]
    mov byte ptr es:[di], '.'
    inc si
    cmp byte ptr [si], '\\'
    je .p83c_dot_sep
    cmp byte ptr [si], '/'
    jne .p83c_dot_ok
.p83c_dot_sep:
    inc si
.p83c_dot_ok:
    mov al, 1
    clc
    jmp .p83c_done

.p83c_dotdot:
    lea di, [name83]
    mov byte ptr es:[di], '.'
    mov byte ptr es:[di + 1], '.'
    add si, 2
    cmp byte ptr [si], '\\'
    je .p83c_dd_sep
    cmp byte ptr [si], '/'
    jne .p83c_dd_ok
.p83c_dd_sep:
    inc si
.p83c_dd_ok:
    mov al, 2
    clc
    jmp .p83c_done

.p83c_normal:
    lea di, [name83]
    mov cx, 8
.p83c_stem:
    mov al, [si]
    test al, al
    jz .p83c_ok
    cmp al, '\\'
    je .p83c_sep
    cmp al, '/'
    je .p83c_sep
    cmp al, '.'
    je .p83c_ext_start
    inc si
    cmp al, 'a'
    jb .p83c_up
    cmp al, 'z'
    ja .p83c_up
    sub al, 0x20
.p83c_up:
    stosb
    loop .p83c_stem
    mov al, [si]
    test al, al
    jz .p83c_ok
    cmp al, '\\'
    je .p83c_sep
    cmp al, '/'
    je .p83c_sep
    cmp al, '.'
    jne .p83c_bad
.p83c_ext_start:
    inc si
    lea di, [name83 + 8]
    mov cx, 3
.p83c_ext:
    mov al, [si]
    test al, al
    jz .p83c_ok
    cmp al, '\\'
    je .p83c_sep
    cmp al, '/'
    je .p83c_sep
    inc si
    cmp al, 'a'
    jb .p83c_ext_up
    cmp al, 'z'
    ja .p83c_ext_up
    sub al, 0x20
.p83c_ext_up:
    stosb
    loop .p83c_ext
    mov al, [si]
    test al, al
    jz .p83c_ok
    cmp al, '\\'
    je .p83c_sep
    cmp al, '/'
    je .p83c_sep
    jmp .p83c_bad

.p83c_sep:
    inc si
.p83c_ok:
    mov al, 0
    clc
    jmp .p83c_done
.p83c_bad:
    stc
.p83c_done:
    pop es
    pop di
    pop cx
    pop bx
    ret

/*
 * AX = parent of directory cluster BP (0 if root / missing "..").
 * Clobbers CS:name83.
 */
path_parent_of_bp:
    push bx
    push cx
    push dx
    push di
    push es
    test bp, bp
    jz .ppo_root
    push cs
    pop es
    lea di, [name83]
    mov cx, 11
    mov al, ' '
    rep stosb
    lea di, [name83]
    mov byte ptr es:[di], '.'
    mov byte ptr es:[di + 1], '.'
    mov dx, bp
    call fat12_find_in_dir
    jnc .ppo_done
.ppo_root:
    xor ax, ax
.ppo_done:
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    ret

/*
 * Resolve ASCIZ path at DS:DX.
 * OUT: DX = parent dir cluster (0=root), CS:name83 = final component,
 *      CS:path_resolve_cluster = DX; CF on error.
 * Blank name83 (11 spaces) = path was "\" or "A:\" (no final component).
 */
path_resolve:
    push ax
    push bx
    push cx
    push si
    push di
    push es
    push bp

    mov si, dx
    xor bh, bh

.pr_sp:
    cmp byte ptr [si], ' '
    jne .pr_sp_done
    inc si
    jmp .pr_sp
.pr_sp_done:

    mov al, [si]
    cmp al, 'A'
    jb .pr_no_drv
    cmp al, 'Z'
    jbe .pr_drv_chk
    cmp al, 'a'
    jb .pr_no_drv
    cmp al, 'z'
    ja .pr_no_drv
.pr_drv_chk:
    cmp byte ptr [si + 1], ':'
    jne .pr_no_drv
    mov al, [si]
    cmp al, 'a'
    jb .pr_drv_up
    cmp al, 'z'
    ja .pr_drv_up
    sub al, 0x20
.pr_drv_up:
    sub al, 'A'
    mov byte ptr cs:[path_resolve_drive], al
    push si
    mov bl, al
    xor bh, bh
    cmp bl, byte ptr cs:[cfg_lastdrive]
    jae .pr_drv_mount
    cmp byte ptr cs:[subst_real + bx], 0xFF
    je .pr_drv_mount
    /* SUBST: mount real drive; remember for abs prefix walk */
    mov al, byte ptr cs:[subst_real + bx]
    mov byte ptr cs:[path_resolve_drive], al
    mov byte ptr cs:[subst_walk], 1
    mov byte ptr cs:[subst_walk_idx], bl
    jmp .pr_drv_mount2
.pr_drv_mount:
    mov byte ptr cs:[subst_walk], 0
.pr_drv_mount2:
    mov dl, al
    call dos_mount_drive
    pop si
    jc .pr_err
    add si, 2
    jmp .pr_after_drv
/* legacy label kept for fallthrough */
.pr_drv:
    jmp .pr_drv_chk
.pr_no_drv:
    /* Relative/default path: ensure current drive volume is mounted. */
    mov al, byte ptr cs:[cur_drive]
    mov byte ptr cs:[path_resolve_drive], al
    mov byte ptr cs:[subst_walk], 0
    push si
    mov bl, al
    xor bh, bh
    cmp bl, byte ptr cs:[cfg_lastdrive]
    jae .pr_no_m
    cmp byte ptr cs:[subst_real + bx], 0xFF
    je .pr_no_m
    mov al, byte ptr cs:[subst_real + bx]
    mov byte ptr cs:[path_resolve_drive], al
    mov byte ptr cs:[subst_walk], 1
    mov byte ptr cs:[subst_walk_idx], bl
.pr_no_m:
    mov dl, al
    call dos_mount_drive
    pop si
    jc .pr_err
.pr_after_drv:

    mov al, [si]
    cmp al, '\\'
    je .pr_abs
    cmp al, '/'
    je .pr_abs
    mov bp, word ptr cs:[cwd_cluster]
    jmp .pr_body
.pr_abs:
    xor bp, bp
    mov bh, 1
    inc si
    /* If SUBST, walk prefix components before remaining path */
    cmp byte ptr cs:[subst_walk], 0
    je .pr_body
    push si
    mov bl, byte ptr cs:[subst_walk_idx]
    xor bh, bh
    mov ax, bx
    mov cl, 6
    shl ax, cl
    push ds
    push cs
    pop ds
    lea si, [subst_prefix]
    add si, ax
    cmp byte ptr [si], '\\'
    jne .pr_pref_ok
    inc si
.pr_pref_ok:
    /* walk prefix dirs into BP; DS=CS */
.pr_pref_loop:
    cmp byte ptr [si], 0
    je .pr_pref_done
    call path_to_83_component
    jc .pr_pref_fail
    cmp al, 1
    je .pr_pref_loop
    cmp al, 2
    je .pr_pref_fail
    mov dx, bp
    call fat12_find_in_dir
    jc .pr_pref_fail
    push ax
    call path_read_attr
    pop dx
    jc .pr_pref_fail
    test al, 0x10
    jz .pr_pref_fail
    mov bp, dx
    jmp .pr_pref_loop
.pr_pref_fail:
    pop ds
    pop si
    jmp .pr_err
.pr_pref_done:
    pop ds
    pop si
    mov bh, 1

.pr_body:
    cmp byte ptr [si], 0
    jne .pr_loop
    test bh, bh
    jz .pr_err
    push cs
    pop es
    lea di, [name83]
    mov cx, 11
    mov al, ' '
    rep stosb
    mov dx, bp
    mov word ptr cs:[path_resolve_cluster], dx
    clc
    jmp .pr_done

.pr_loop:
    call path_to_83_component
    jc .pr_err
    mov bl, al
    cmp byte ptr [si], 0
    je .pr_final

    cmp bl, 1
    je .pr_loop
    cmp bl, 2
    jne .pr_nf_dir
    call path_parent_of_bp
    mov bp, ax
    jmp .pr_loop

.pr_nf_dir:
    mov dx, bp
    call fat12_find_in_dir
    jc .pr_err
    push ax
    call path_read_attr
    pop dx
    jc .pr_err
    test al, 0x10
    jz .pr_err
    mov bp, dx
    jmp .pr_loop

.pr_err:
    mov dx, bp
    mov word ptr cs:[path_resolve_cluster], dx
    stc
    jmp .pr_done

.pr_final:
    cmp bl, 1
    je .pr_final_dot
    cmp bl, 2
    je .pr_final_dd
    mov dx, bp
    mov word ptr cs:[path_resolve_cluster], dx
    clc
    jmp .pr_done

.pr_final_dot:
    /* "." as final component → stay in dir; blank name83 */
    push ax
    push cx
    push di
    push es
    push cs
    pop es
    lea di, [name83]
    mov cx, 11
    mov al, ' '
    rep stosb
    pop es
    pop di
    pop cx
    pop ax
    mov dx, bp
    mov word ptr cs:[path_resolve_cluster], dx
    clc
    jmp .pr_done

.pr_final_dd:
    call path_parent_of_bp
    mov bp, ax
    push ax
    push cx
    push di
    push es
    push cs
    pop es
    lea di, [name83]
    mov cx, 11
    mov al, ' '
    rep stosb
    pop es
    pop di
    pop cx
    pop ax
    mov dx, bp
    mov word ptr cs:[path_resolve_cluster], dx
    clc
    jmp .pr_done

.pr_done:
    pop bp
    pop es
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    ret

path_name83_present:
    push ax
    push cx
    push si
    push ds
    push cs
    pop ds
    lea si, [name83]
    mov cx, 11
.pnp:
    lodsb
    cmp al, ' '
    jne .pnp_yes
    loop .pnp
    stc
    jmp .pnp_done
.pnp_yes:
    clc
.pnp_done:
    pop ds
    pop si
    pop cx
    pop ax
    ret

path_read_attr:
    push bx
    push cx
    push di
    push es
    push cs
    pop es
    mov ax, word ptr cs:[last_dir_lba]
    lea bx, [sector_buf]
    xor dx, dx
    call fat12_read_lba
    jc .pra_err
    mov bx, word ptr cs:[last_dir_idx]
    mov cl, 5
    shl bx, cl
    lea di, [sector_buf]
    add di, bx
    mov al, es:[di + 11]
    clc
    jmp .pra_done
.pra_err:
    stc
.pra_done:
    pop es
    pop di
    pop cx
    pop bx
    ret

path_write_attr:
    push ax
    push bx
    push cx
    push di
    push es
    push cs
    pop es
    mov bx, word ptr cs:[last_dir_idx]
    mov cl, 5
    shl bx, cl
    lea di, [sector_buf]
    add di, bx
    mov es:[di + 11], al
    mov ax, word ptr cs:[last_dir_lba]
    lea bx, [sector_buf]
    xor dx, dx
    call fat12_write_lba
    pop es
    pop di
    pop cx
    pop bx
    pop ax
    ret

path_dir_is_empty:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push bp
    mov bp, ax
    push cs
    pop es
    xor dx, dx
.pde_sec:
    mov bx, dx
    push dx
    mov dx, bp
    call fat12_dir_sector
    jc .pde_ok_pop
    lea bx, [sector_buf]
    call fat12_read_lba
    pop dx
    jc .pde_bad
    xor si, si
.pde_ent:
    cmp si, 16
    jae .pde_next
    push bx
    mov bx, si
    mov cl, 5
    shl bx, cl
    lea di, [sector_buf]
    add di, bx
    pop bx
    mov al, es:[di]
    test al, al
    jz .pde_ok
    cmp al, 0xE5
    je .pde_skip
    cmp al, '.'
    jne .pde_bad
    mov al, es:[di + 1]
    cmp al, ' '
    je .pde_skip
    cmp al, '.'
    jne .pde_bad
    cmp byte ptr es:[di + 2], ' '
    jne .pde_bad
.pde_skip:
    inc si
    jmp .pde_ent
.pde_next:
    inc dx
    jmp .pde_sec
.pde_ok_pop:
    pop dx
.pde_ok:
    clc
    jmp .pde_done
.pde_bad:
    stc
.pde_done:
    pop bp
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

path_write_dot_entries:
    push ax
    push bx
    push cx
    push dx
    push di
    push es
    push bp
    mov bp, ax
    push dx
    call fat12_zero_cluster
    jc .pwd_fail
    push cs
    pop es
    lea di, [sector_buf]
    xor ax, ax
    mov cx, 256
    rep stosw
    lea di, [sector_buf]
    mov cx, 11
    mov al, ' '
    push di
    rep stosb
    pop di
    mov byte ptr es:[di], '.'
    mov byte ptr es:[di + 11], 0x10
    mov word ptr es:[di + 26], bp
    add di, 32
    mov cx, 11
    mov al, ' '
    push di
    rep stosb
    pop di
    mov byte ptr es:[di], '.'
    mov byte ptr es:[di + 1], '.'
    mov byte ptr es:[di + 11], 0x10
    pop ax
    mov word ptr es:[di + 26], ax
    mov ax, bp
    call fat12_cluster_to_lba
    lea bx, [sector_buf]
    call fat12_write_lba
    jmp .pwd_done
.pwd_fail:
    pop ax                       /* discard parent cluster */
    stc
.pwd_done:
    pop bp
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * Append CS:name83 as \NAME[.EXT] at ES:DI (ASCIZ builder). DI advanced.
 * CF if would exceed cwd_path+63.
 */
path_append_name83:
    push ax
    push bx
    push cx
    push si
    push ds
    lea bx, [cwd_path + 63]
    cmp di, bx
    jae .pan_err
    mov byte ptr es:[di], '\\'
    inc di
    push cs
    pop ds
    lea si, [name83]
    mov cx, 8
.pan_stem:
    lodsb
    cmp al, ' '
    je .pan_stem_done
    cmp di, bx
    jae .pan_err
    mov es:[di], al
    inc di
    loop .pan_stem
.pan_stem_done:
    lea si, [name83 + 8]
    cmp byte ptr [si], ' '
    je .pan_ok
    cmp di, bx
    jae .pan_err
    mov byte ptr es:[di], '.'
    inc di
    mov cx, 3
.pan_ext:
    lodsb
    cmp al, ' '
    je .pan_ok
    cmp di, bx
    jae .pan_err
    mov es:[di], al
    inc di
    loop .pan_ext
.pan_ok:
    mov byte ptr es:[di], 0
    clc
    jmp .pan_done
.pan_err:
    stc
.pan_done:
    pop ds
    pop si
    pop cx
    pop bx
    pop ax
    ret

/* Strip last \COMPONENT from ASCIZ at CS:cwd_path; DI = new end. */
path_strip_last:
    push ax
    push bx
    push ds
    push cs
    pop ds
    lea bx, [cwd_path]
    lea di, [cwd_path]
.psl_end:
    cmp byte ptr [di], 0
    je .psl_scan
    inc di
    jmp .psl_end
.psl_scan:
    cmp di, bx
    jbe .psl_empty
    dec di
    cmp byte ptr [di], '\\'
    je .psl_cut
    cmp di, bx
    ja .psl_scan
.psl_empty:
    lea di, [cwd_path]
    mov byte ptr [di], 0
    jmp .psl_done
.psl_cut:
    mov byte ptr [di], 0
.psl_done:
    pop ds
    pop bx
    pop ax
    ret

/*
 * Rebuild CS:cwd_path from ASCIZ path at DS:DX after successful CHDIR.
 */
path_rebuild_cwd:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds
    push bp

    mov si, dx
    push cs
    pop es

.prc_sp:
    cmp byte ptr ds:[si], ' '
    jne .prc_sp_done
    inc si
    jmp .prc_sp
.prc_sp_done:

    mov al, ds:[si]
    cmp al, 'A'
    je .prc_drv
    cmp al, 'a'
    je .prc_drv
    jmp .prc_no_drv
.prc_drv:
    cmp byte ptr ds:[si + 1], ':'
    jne .prc_no_drv
    add si, 2
.prc_no_drv:

    mov al, ds:[si]
    cmp al, '\\'
    je .prc_abs
    cmp al, '/'
    je .prc_abs

    /* relative: DI = end of cwd_path */
    push ds
    push cs
    pop ds
    lea di, [cwd_path]
.prc_zend:
    cmp byte ptr [di], 0
    je .prc_zend_done
    inc di
    jmp .prc_zend
.prc_zend_done:
    pop ds
    jmp .prc_comps

.prc_abs:
    inc si
    lea di, [cwd_path]
    mov byte ptr es:[di], 0

.prc_comps:
    cmp byte ptr ds:[si], 0
    je .prc_ok
    call path_to_83_component
    jc .prc_ok
    cmp al, 1
    je .prc_comps
    cmp al, 2
    jne .prc_add
    call path_strip_last
    jmp .prc_comps
.prc_add:
    call path_append_name83
    jc .prc_ok
    jmp .prc_comps

.prc_ok:
    pop bp
    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* ---------- INT 21h handlers ---------- */

dos_ah39:
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push bp

    call path_resolve
    jc .a39_err
    call path_name83_present
    jc .a39_err
    mov bp, dx

    call fat12_find_in_dir
    jnc .a39_err

    mov dx, bp
    call fat12_find_free_in_dir
    jc .a39_err
    mov al, 0x10
    call fat12_create_dirent_attr

    call fat12_alloc_cluster
    jc .a39_err
    mov bx, ax
    xor dx, dx
    mov word ptr cs:[last_size_hi], 0
    call fat12_update_dirent
    jc .a39_err

    mov ax, bx
    mov dx, bp
    call path_write_dot_entries
    jc .a39_err
    call fat12_flush_fat
    jc .a39_err

    call dos_remount_current
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    xor ax, ax
    clc
    jmp int21_iret

.a39_err:
    call dos_remount_current
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov ax, 3
    stc
    jmp int21_iret_cf

dos_ah3a:
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push bp

    call path_resolve
    jc .a3a_err
    call path_name83_present
    jc .a3a_err
    call fat12_find_in_dir
    jc .a3a_err
    mov bp, ax
    call path_read_attr
    jc .a3a_err
    test al, 0x10
    jz .a3a_err
    test bp, bp
    jz .a3a_err
    mov ax, bp
    call path_dir_is_empty
    jc .a3a_err
    mov ax, bp
    call fat12_free_chain
    call fat12_delete_dirent
    jc .a3a_err
    call fat12_flush_fat
    jc .a3a_err

    call dos_remount_current
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    xor ax, ax
    clc
    jmp int21_iret

.a3a_err:
    call dos_remount_current
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov ax, 3
    stc
    jmp int21_iret_cf

dos_ah3b:
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push bp

    mov bp, dx
    mov si, dx
.a3b_skip_sp:
    cmp byte ptr [si], ' '
    jne .a3b_sp_done
    inc si
    jmp .a3b_skip_sp
.a3b_sp_done:
    /* bare "A:" or empty → no-op success (FIND cwd restore at root) */
    cmp byte ptr [si], 0
    je .a3b_noop
    cmp byte ptr [si + 1], ':'
    jne .a3b_resolve
    cmp byte ptr [si + 2], 0
    je .a3b_noop
.a3b_resolve:
    call path_resolve
    jc .a3b_err
    call path_name83_present
    jc .a3b_root
    call fat12_find_in_dir
    jc .a3b_err
    mov bx, ax
    call path_read_attr
    jc .a3b_err
    test al, 0x10
    jz .a3b_err
    mov word ptr cs:[cwd_cluster], bx
    jmp .a3b_path
.a3b_root:
    mov word ptr cs:[cwd_cluster], dx
.a3b_path:
    /* CD with drive letter adopts that drive (unlike MD/COPY). */
    mov al, byte ptr cs:[path_resolve_drive]
    mov byte ptr cs:[cur_drive], al
    mov dx, bp
    call path_rebuild_cwd
    call dos_cwd_save_current
.a3b_noop:
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    xor ax, ax
    clc
    jmp int21_iret

.a3b_err:
    call dos_remount_current
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov ax, 3
    stc
    jmp int21_iret_cf

dos_ah47:
    push cx
    push si
    push di
    push es
    push ds
    push ds
    pop es
    mov di, si
    push cs
    pop ds
    lea si, [cwd_path]
    cmp byte ptr [si], '\\'
    jne .a47_copy
    inc si
.a47_copy:
    lodsb
    stosb
    test al, al
    jnz .a47_copy
    pop ds
    pop es
    pop di
    pop si
    pop cx
    xor ax, ax
    clc
    jmp int21_iret

dos_ah43:
    push bx
    push dx
    push si
    push di
    push ds
    push es
    push bp
    push cx                       /* CL = set-attr (AL=1) */

    mov bp, ax                    /* AL = 0 get / 1 set */
    call path_resolve
    jc .a43_err_pop
    call path_name83_present
    jc .a43_err_pop
    call fat12_find_in_dir
    jc .a43_err_pop
    call path_read_attr
    jc .a43_err_pop

    test bp, 0x00FF
    jnz .a43_set
    mov cl, al
    xor ch, ch
    pop ax                        /* discard saved CX */
    jmp .a43_ok

.a43_set:
    pop dx                        /* DL = requested attr */
    mov ah, al
    mov al, dl
    and al, 0x27
    test ah, 0x10
    jz .a43_set_go
    or al, 0x10
.a43_set_go:
    call path_write_attr
    jc .a43_err

.a43_ok:
    call dos_remount_current
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop bx
    xor ax, ax
    clc
    jmp int21_iret

.a43_err_pop:
    pop cx
.a43_err:
    call dos_remount_current
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop bx
    mov ax, 2
    stc
    jmp int21_iret_cf

/*
 * AH=56h rename — DS:DX=old, ES:DI=new.
 * Same parent directory required; rewrites 11-byte name in dirent.
 */
dos_ah56:
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    push bp

    mov bp, di                   /* new path offset */
    mov si, es                   /* new path segment */

    call path_resolve
    jc .a56_err
    call path_name83_present
    jc .a56_err
    call fat12_find_in_dir
    jc .a56_err
    mov bx, dx                   /* old parent */
    push word ptr cs:[last_dir_lba]
    push word ptr cs:[last_dir_idx]

    push ds
    mov ds, si
    mov dx, bp
    push bx
    call path_resolve
    pop bx
    pop ds
    jc .a56_fail_pop2
    call path_name83_present
    jc .a56_fail_pop2
    cmp dx, bx
    jne .a56_fail_pop2
    push bx
    call fat12_find_in_dir
    pop bx
    jnc .a56_fail_pop2

    pop word ptr cs:[last_dir_idx]
    pop word ptr cs:[last_dir_lba]

    push cs
    pop es
    mov ax, word ptr cs:[last_dir_lba]
    lea bx, [sector_buf]
    xor dx, dx
    call fat12_read_lba
    jc .a56_err
    mov bx, word ptr cs:[last_dir_idx]
    mov cl, 5
    shl bx, cl
    lea di, [sector_buf]
    add di, bx
    push di
    push ds
    push cs
    pop ds
    lea si, [name83]
    mov cx, 11
    rep movsb
    pop ds
    pop di
    mov ax, word ptr cs:[last_dir_lba]
    lea bx, [sector_buf]
    xor dx, dx
    call fat12_write_lba
    jc .a56_err

    call dos_remount_current
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    xor ax, ax
    clc
    jmp int21_iret

.a56_fail_pop2:
    add sp, 4
.a56_err:
    call dos_remount_current
    pop bp
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    mov ax, 2
    stc
    jmp int21_iret_cf
