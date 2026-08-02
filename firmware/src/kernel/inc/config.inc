/*
 * CONFIG.SYS — parse before COMMAND.COM.
 * Supports INSTALL= (load+run COM), DEVICE= (.SYS via driver ABI, else COM),
 * FILES=, BUFFERS=, SHELL=. Missing CONFIG.SYS is success (no-op).
 */

.equ CFG_LINE_MAX, 120
.equ CFG_FILES_DEFAULT, 20
.equ CFG_BUFS_DEFAULT, 8

dos_process_config:
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
    mov word ptr [cfg_files], CFG_FILES_DEFAULT
    mov word ptr [cfg_buffers], CFG_BUFS_DEFAULT

    mov ah, 0x3D
    xor al, al
    lea dx, [path_config]
    int 0x21
    jc .cfg_done_ok
    mov word ptr [cfg_handle], ax

.cfg_line:
    call cfg_read_line
    jc .cfg_close
    /* empty / comment */
    lea si, [cfg_line]
    call cfg_skip_ws
    cmp byte ptr [si], 0
    je .cfg_line
    cmp byte ptr [si], ';'
    je .cfg_line

    call cfg_match_install
    jnc .cfg_do_install
    call cfg_match_device
    jnc .cfg_do_device
    call cfg_match_files
    jnc .cfg_line
    call cfg_match_buffers
    jnc .cfg_line
    call cfg_match_shell
    jnc .cfg_line
    jmp .cfg_line

.cfg_do_device:
    /* SI → path; .SYS → load_device_sys, else COM load+run */
    push ds
    push cs
    pop ds
    call cfg_path_is_sys
    jc .cfg_dev_com
    mov dx, si
    call load_device_sys
    pop ds
    jnc .cfg_line
    jmp .cfg_install_fail
.cfg_dev_com:
    mov dx, si
    call load_and_run_com
    pop ds
    jnc .cfg_line
    jmp .cfg_install_fail

.cfg_do_install:
    /* SI → path ASCIZ in cfg_line (already advanced) */
    push ds
    push cs
    pop ds
    mov dx, si
    call load_and_run_com
    pop ds
    jnc .cfg_line
.cfg_install_fail:
    mov ah, 0x09
    lea dx, [msg_cfg_install]
    int 0x21
    jmp .cfg_line

.cfg_close:
    mov ah, 0x3E
    mov bx, word ptr [cfg_handle]
    int 0x21
.cfg_done_ok:
    clc
    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* Read one CR/LF-terminated line into cfg_line (NUL-ended). CF=EOF. */
cfg_read_line:
    push ax
    push bx
    push cx
    push dx
    push di
    lea di, [cfg_line]
    mov cx, CFG_LINE_MAX - 1
.cfg_rl_loop:
    push cx
    mov ah, 0x3F
    mov bx, word ptr [cfg_handle]
    mov cx, 1
    lea dx, [cfg_ch]
    int 0x21
    pop cx
    jc .cfg_rl_eof
    cmp ax, 1
    jne .cfg_rl_eof
    mov al, byte ptr [cfg_ch]
    cmp al, 13
    je .cfg_rl_cr
    cmp al, 10
    je .cfg_rl_end
    mov [di], al
    inc di
    loop .cfg_rl_loop
    jmp .cfg_rl_end
.cfg_rl_cr:
    /* peek LF */
    mov ah, 0x3F
    mov bx, word ptr [cfg_handle]
    mov cx, 1
    lea dx, [cfg_ch]
    int 0x21
    jmp .cfg_rl_end
.cfg_rl_eof:
    lea bx, [cfg_line]
    cmp di, bx
    je .cfg_rl_fail
.cfg_rl_end:
    mov byte ptr [di], 0
    clc
    jmp .cfg_rl_done
.cfg_rl_fail:
    stc
.cfg_rl_done:
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

cfg_skip_ws:
.cfg_sw:
    cmp byte ptr [si], ' '
    je .cfg_sw_inc
    cmp byte ptr [si], 9
    je .cfg_sw_inc
    ret
.cfg_sw_inc:
    inc si
    jmp .cfg_sw

/* Uppercase [SI] in place until NUL. */
cfg_upcase_line:
    push ax
    push si
.cfg_up:
    mov al, [si]
    test al, al
    jz .cfg_up_done
    cmp al, 'a'
    jb .cfg_up_next
    cmp al, 'z'
    ja .cfg_up_next
    sub al, 32
    mov [si], al
.cfg_up_next:
    inc si
    jmp .cfg_up
.cfg_up_done:
    pop si
    pop ax
    ret

/* Match INSTALL= → CF clear, SI=path. */
cfg_match_install:
    push ax
    push di
    lea si, [cfg_line]
    call cfg_skip_ws
    call cfg_upcase_line
    lea di, [cfg_kw_install]
    call cfg_strcmp_prefix
    jc .cfg_mi_no
    cmp byte ptr [si], '='
    jne .cfg_mi_no
    inc si
    call cfg_skip_ws
    call cfg_trim_path
    clc
    jmp .cfg_mi_done
.cfg_mi_no:
    stc
.cfg_mi_done:
    pop di
    pop ax
    ret

cfg_match_device:
    push ax
    push di
    lea si, [cfg_line]
    call cfg_skip_ws
    call cfg_upcase_line
    lea di, [cfg_kw_device]
    call cfg_strcmp_prefix
    jc .cfg_md_no
    cmp byte ptr [si], '='
    jne .cfg_md_no
    inc si
    call cfg_skip_ws
    call cfg_trim_path
    clc
    jmp .cfg_md_done
.cfg_md_no:
    stc
.cfg_md_done:
    pop di
    pop ax
    ret

cfg_match_files:
    push ax
    push di
    lea si, [cfg_line]
    call cfg_skip_ws
    call cfg_upcase_line
    lea di, [cfg_kw_files]
    call cfg_strcmp_prefix
    jc .cfg_mf_no
    cmp byte ptr [si], '='
    jne .cfg_mf_no
    inc si
    call cfg_skip_ws
    call cfg_parse_u16
    mov word ptr [cfg_files], ax
    clc
    jmp .cfg_mf_done
.cfg_mf_no:
    stc
.cfg_mf_done:
    pop di
    pop ax
    ret

cfg_match_buffers:
    push ax
    push di
    lea si, [cfg_line]
    call cfg_skip_ws
    call cfg_upcase_line
    lea di, [cfg_kw_buffers]
    call cfg_strcmp_prefix
    jc .cfg_mb_no
    cmp byte ptr [si], '='
    jne .cfg_mb_no
    inc si
    call cfg_skip_ws
    call cfg_parse_u16
    mov word ptr [cfg_buffers], ax
    clc
    jmp .cfg_mb_done
.cfg_mb_no:
    stc
.cfg_mb_done:
    pop di
    pop ax
    ret

cfg_match_shell:
    push ax
    push di
    lea si, [cfg_line]
    call cfg_skip_ws
    call cfg_upcase_line
    lea di, [cfg_kw_shell]
    call cfg_strcmp_prefix
    jc .cfg_ms_no
    cmp byte ptr [si], '='
    jne .cfg_ms_no
    inc si
    call cfg_skip_ws
    call cfg_trim_path
    /* copy path into path_command */
    push es
    push cs
    pop es
    lea di, [path_command]
.cfg_ms_copy:
    lodsb
    stosb
    test al, al
    jnz .cfg_ms_copy
    pop es
    clc
    jmp .cfg_ms_done
.cfg_ms_no:
    stc
.cfg_ms_done:
    pop di
    pop ax
    ret

/* DI=keyword ASCIZ, SI=line. Advance SI past keyword on match. CF=no. */
cfg_strcmp_prefix:
    push ax
    push bx
    push di
.cfg_sp:
    mov al, [di]
    test al, al
    jz .cfg_sp_ok
    mov bl, [si]
    cmp al, bl
    jne .cfg_sp_fail
    inc si
    inc di
    jmp .cfg_sp
.cfg_sp_ok:
    clc
    jmp .cfg_sp_done
.cfg_sp_fail:
    stc
.cfg_sp_done:
    pop di
    pop bx
    pop ax
    ret

/* SI=digits → AX. */
cfg_parse_u16:
    xor ax, ax
.cfg_pu:
    mov bl, [si]
    cmp bl, '0'
    jb .cfg_pu_done
    cmp bl, '9'
    ja .cfg_pu_done
    mov cx, 10
    mul cx
    xor bh, bh
    sub bl, '0'
    add ax, bx
    inc si
    jmp .cfg_pu
.cfg_pu_done:
    ret

/* Trim trailing spaces/CR at SI path; ensure ASCIZ. */
cfg_trim_path:
    push ax
    push di
    mov di, si
.cfg_tp_find:
    cmp byte ptr [di], 0
    je .cfg_tp_back
    inc di
    jmp .cfg_tp_find
.cfg_tp_back:
    cmp di, si
    je .cfg_tp_done
    dec di
    mov al, [di]
    cmp al, ' '
    je .cfg_tp_zap
    cmp al, 9
    je .cfg_tp_zap
    cmp al, 13
    je .cfg_tp_zap
    jmp .cfg_tp_done
.cfg_tp_zap:
    mov byte ptr [di], 0
    jmp .cfg_tp_back
.cfg_tp_done:
    pop di
    pop ax
    ret
