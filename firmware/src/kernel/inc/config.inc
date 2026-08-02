/*
 * CONFIG.SYS — parse before COMMAND.COM.
 * Supports INSTALL= (load+run COM), character-device DEVICE= (.SYS), FILES=,
 * BUFFERS=, LASTDRIVE=, BREAK=, and SHELL=. Missing CONFIG.SYS is success.
 */

.equ CFG_LINE_MAX, 120
.equ CFG_FILES_DEFAULT, 20
.equ CFG_BUFS_DEFAULT, 8
.equ CFG_LASTDRIVE_DEFAULT, 8

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
    mov byte ptr [cfg_lastdrive], CFG_LASTDRIVE_DEFAULT

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
    call cfg_match_lastdrive
    jnc .cfg_line
    call cfg_match_break
    jnc .cfg_line
    call cfg_match_shell
    jnc .cfg_line
    call cfg_print_ignored
    jmp .cfg_line

.cfg_do_device:
    /* DEVICE= is limited to character .SYS drivers; no block-driver ABI. */
    push ds
    push cs
    pop ds
    call cfg_path_is_sys
    jc .cfg_dev_not_sys
    mov dx, si
    call load_device_sys
    pop ds
    jnc .cfg_line
    call cfg_print_device_fail
    jmp .cfg_line
.cfg_dev_not_sys:
    pop ds
    mov ah, 0x09
    lea dx, [msg_cfg_device_sys]
    int 0x21
    jmp .cfg_line

.cfg_do_install:
    /* Split INSTALL=path args and expose args through an EXEC parameter block. */
    push ds
    push cs
    pop ds
    call cfg_prepare_install
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
    lea di, [cfg_kw_install]
    call cfg_stricmp_prefix
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

/*
 * SI = INSTALL value. Terminate the executable path at the first whitespace,
 * then build a conventional EXEC tail (leading space, args, CR) and parameter
 * block for load_and_run_com.
 */
cfg_prepare_install:
    push ax
    push bx
    push cx
    push di

    mov di, si
.cfg_pi_path:
    mov al, [di]
    test al, al
    jz .cfg_pi_no_args
    cmp al, ' '
    je .cfg_pi_split
    cmp al, 9
    je .cfg_pi_split
    inc di
    jmp .cfg_pi_path

.cfg_pi_split:
    mov byte ptr [di], 0
    inc di
.cfg_pi_skip:
    cmp byte ptr [di], ' '
    je .cfg_pi_skip_one
    cmp byte ptr [di], 9
    jne .cfg_pi_args
.cfg_pi_skip_one:
    inc di
    jmp .cfg_pi_skip

.cfg_pi_args:
    cmp byte ptr [di], 0
    je .cfg_pi_no_args
    lea bx, [cfg_install_tail]
    mov byte ptr [bx + 1], ' '
    add bx, 2
    mov cx, 1
.cfg_pi_copy:
    mov al, [di]
    test al, al
    jz .cfg_pi_tail_done
    cmp cx, 127
    jae .cfg_pi_tail_done
    mov [bx], al
    inc bx
    inc di
    inc cx
    jmp .cfg_pi_copy

.cfg_pi_no_args:
    lea bx, [cfg_install_tail]
    xor cx, cx

.cfg_pi_tail_done:
    lea bx, [cfg_install_tail]
    mov byte ptr [bx], cl
    add bx, cx
    mov byte ptr [bx + 1], 0x0D

    mov word ptr [cfg_install_pb], 0
    lea ax, [cfg_install_tail]
    mov word ptr [cfg_install_pb + 2], ax
    mov word ptr [cfg_install_pb + 4], cs
    lea ax, [cfg_install_fcb1]
    mov word ptr [cfg_install_pb + 6], ax
    mov word ptr [cfg_install_pb + 8], cs
    lea ax, [cfg_install_fcb2]
    mov word ptr [cfg_install_pb + 0x0A], ax
    mov word ptr [cfg_install_pb + 0x0C], cs

    mov word ptr [exec_pb_seg], cs
    lea ax, [cfg_install_pb]
    mov word ptr [exec_pb_off], ax
    mov byte ptr [exec_pb_valid], 1

    pop di
    pop cx
    pop bx
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

/* Match LASTDRIVE=letter|count and store a count in 1..CDS_COUNT. */
cfg_match_lastdrive:
    push ax
    push di
    lea si, [cfg_line]
    call cfg_skip_ws
    call cfg_upcase_line
    lea di, [cfg_kw_lastdrive]
    call cfg_strcmp_prefix
    jc .cfg_ml_no
    cmp byte ptr [si], '='
    jne .cfg_ml_no
    inc si
    call cfg_skip_ws
    call cfg_trim_path
    mov al, [si]
    cmp al, 'A'
    jb .cfg_ml_count
    cmp al, 'Z'
    ja .cfg_ml_count
    sub al, 'A'
    inc al
    cmp byte ptr [si + 1], 0
    jne .cfg_ml_no
    jmp .cfg_ml_store
.cfg_ml_count:
    call cfg_parse_u16
    test ax, ax
    jz .cfg_ml_no
    cmp ax, CDS_COUNT
    jbe .cfg_ml_count_ok
    mov ax, CDS_COUNT
.cfg_ml_count_ok:
    mov al, al
.cfg_ml_store:
    mov byte ptr [cfg_lastdrive], al
    clc
    jmp .cfg_ml_done
.cfg_ml_no:
    stc
.cfg_ml_done:
    pop di
    pop ax
    ret

/* Match BREAK=ON|OFF and wire it to AH=33h's break_flag. */
cfg_match_break:
    push ax
    push di
    lea si, [cfg_line]
    call cfg_skip_ws
    call cfg_upcase_line
    lea di, [cfg_kw_break]
    call cfg_strcmp_prefix
    jc .cfg_mbr_no
    cmp byte ptr [si], '='
    jne .cfg_mbr_no
    inc si
    call cfg_skip_ws
    call cfg_trim_path
    cmp byte ptr [si], 'O'
    jne .cfg_mbr_no
    cmp byte ptr [si + 1], 'N'
    jne .cfg_mbr_off
    cmp byte ptr [si + 2], 0
    jne .cfg_mbr_no
    mov byte ptr [break_flag], 1
    clc
    jmp .cfg_mbr_done
.cfg_mbr_off:
    cmp byte ptr [si + 1], 'F'
    jne .cfg_mbr_no
    cmp byte ptr [si + 2], 'F'
    jne .cfg_mbr_no
    cmp byte ptr [si + 3], 0
    jne .cfg_mbr_no
    mov byte ptr [break_flag], 0
    clc
    jmp .cfg_mbr_done
.cfg_mbr_no:
    stc
.cfg_mbr_done:
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

/* DI=uppercase keyword, SI=line. Case-insensitive prefix compare; CF=no match. */
cfg_stricmp_prefix:
    push ax
    push bx
    push di
.cfg_sip:
    mov al, [di]
    test al, al
    jz .cfg_sip_ok
    mov bl, [si]
    cmp bl, 'a'
    jb .cfg_sip_cmp
    cmp bl, 'z'
    ja .cfg_sip_cmp
    sub bl, 32
.cfg_sip_cmp:
    cmp al, bl
    jne .cfg_sip_fail
    inc si
    inc di
    jmp .cfg_sip
.cfg_sip_ok:
    clc
    jmp .cfg_sip_done
.cfg_sip_fail:
    stc
.cfg_sip_done:
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

/* Print the unsupported directive exactly as parsed. */
cfg_print_ignored:
    push ax
    push dx
    push si
    mov ah, 0x09
    lea dx, [msg_cfg_ignored]
    int 0x21
    lea si, [cfg_line]
.cfg_pi_loop:
    mov dl, [si]
    test dl, dl
    jz .cfg_pi_done
    mov ah, 0x02
    int 0x21
    inc si
    jmp .cfg_pi_loop
.cfg_pi_done:
    mov ah, 0x09
    lea dx, [msg_cfg_crlf]
    int 0x21
    pop si
    pop dx
    pop ax
    ret

cfg_print_device_fail:
    push ax
    push dx
    mov al, byte ptr [device_load_error]
    cmp al, 1
    je .cfg_pdf_size
    cmp al, 2
    je .cfg_pdf_char
    lea dx, [msg_cfg_device_failed]
    jmp .cfg_pdf_print
.cfg_pdf_size:
    lea dx, [msg_cfg_device_size]
    jmp .cfg_pdf_print
.cfg_pdf_char:
    lea dx, [msg_cfg_device_char]
.cfg_pdf_print:
    mov ah, 0x09
    int 0x21
    pop dx
    pop ax
    ret
