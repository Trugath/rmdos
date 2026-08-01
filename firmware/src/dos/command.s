.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * COMMAND.COM — DIR/TYPE/COPY/DEL/CLS/CD/MD/RD/ECHO/IF ERRORLEVEL
 * + exec with command tail; runs AUTOEXEC.BAT once at startup.
 */

_start:
    push cs
    pop ds
    push cs
    pop es

    call run_autoexec

.repl:
    push cs
    pop ds
    push cs
    pop es
    cld

    call show_prompt

    /* Fresh DOS input buffer: never reuse prior length/chars as a template. */
    mov byte ptr [line], 80
    mov byte ptr [line + 1], 0
    lea di, [line + 2]
    mov cx, 80
    xor al, al
    rep stosb

    mov ah, 0x0A
    lea dx, [line]
    int 0x21

    push cs
    pop ds
    cld

    xor ch, ch
    mov cl, [line + 1]
    mov byte ptr [cmd], 0
    test cl, cl
    jz .repl                    /* empty line: NOP */
    lea si, [line + 2]
    lea di, [cmd]
    rep movsb
    mov byte ptr [di], 0

    /* Whitespace-only is also a NOP */
    lea si, [cmd]
    call skip_spaces
    cmp byte ptr [si], 0
    je .repl

    call dispatch
    jmp .repl

/*
 * Dispatch ASCIZ command in [cmd].  The wrapper handles the shell syntax
 * which applies to both interactive and batch input before the old builtin
 * dispatcher sees the command.
 */
dispatch:
    call pipe_command
    jc .no_pipe
    ret
.no_pipe:
    call parse_redirection
    call setup_redirection
    jc .redirection_done
    call dispatch_plain
    call restore_redirection
.redirection_done:
    ret

/* Split one pipe, run its left side into PIPE.$$$, then feed its right side. */
pipe_command:
    push ax
    push si
    push di
    lea si, [cmd]
.pc_find:
    mov al, [si]
    test al, al
    jz .pc_none
    cmp al, '|'
    je .pc_found
    inc si
    jmp .pc_find
.pc_found:
    mov byte ptr [si], 0
    inc si
    lea di, [pipe_rhs]
.pc_copy:
    lodsb
    stosb
    test al, al
    jnz .pc_copy
    lea dx, [path_pipe]
    call run_with_stdout
    lea si, [pipe_rhs]
    lea di, [cmd]
.pc_rhs_copy:
    lodsb
    stosb
    test al, al
    jnz .pc_rhs_copy
    lea dx, [path_pipe]
    call run_with_stdin
    mov ah, 0x41
    lea dx, [path_pipe]
    int 0x21
    clc
    jmp .pc_done
.pc_none:
    stc
.pc_done:
    pop di
    pop si
    pop ax
    ret

/* Run current cmd with stdout/stdin forced to DS:DX, restoring it after. */
run_with_stdout:
    push dx
    mov ah, 0x3C
    xor cx, cx
    int 0x21
    jc .rwso_done
    mov [redir_handle], bx
    mov bx, 1
    mov ah, 0x45
    int 0x21
    jc .rwso_close
    mov [saved_stdout], ax
    mov bx, [redir_handle]
    mov cx, 1
    mov ah, 0x46
    int 0x21
    call dispatch_plain
    call restore_stdout
.rwso_close:
    mov bx, [redir_handle]
    mov ah, 0x3E
    int 0x21
.rwso_done:
    pop dx
    ret

run_with_stdin:
    push dx
    mov ax, 0x3D00
    int 0x21
    jc .rwsi_done
    mov [redir_handle], bx
    xor bx, bx
    mov ah, 0x45
    int 0x21
    jc .rwsi_close
    mov [saved_stdin], ax
    mov bx, [redir_handle]
    xor cx, cx
    mov ah, 0x46
    int 0x21
    call dispatch_plain
    call restore_stdin
.rwsi_close:
    mov bx, [redir_handle]
    mov ah, 0x3E
    int 0x21
.rwsi_done:
    pop dx
    ret

/* Detect a final <, >, or >> clause.  The command is truncated in place. */
parse_redirection:
    mov byte ptr [redir_kind], 0
    lea si, [cmd]
.pr_scan:
    mov al, [si]
    test al, al
    jz .pr_done
    cmp al, '>'
    je .pr_out
    cmp al, '<'
    je .pr_in
    inc si
    jmp .pr_scan
.pr_out:
    mov byte ptr [si], 0
    mov byte ptr [redir_kind], 1
    inc si
    cmp byte ptr [si], '>'
    jne .pr_out_name
    mov byte ptr [redir_kind], 2
    inc si
.pr_out_name:
    call skip_spaces
    lea di, [redir_name]
    jmp .pr_copy
.pr_in:
    mov byte ptr [si], 0
    mov byte ptr [redir_kind], 3
    inc si
    call skip_spaces
    lea di, [redir_name]
.pr_copy:
    lodsb
    test al, al
    jz .pr_term
    cmp al, ' '
    je .pr_term
    stosb
    jmp .pr_copy
.pr_term:
    mov byte ptr [di], 0
.pr_done:
    ret

setup_redirection:
    cmp byte ptr [redir_kind], 0
    je .sr_ok
    cmp byte ptr [redir_kind], 3
    je .sr_input
    mov ax, 0x3C00
    cmp byte ptr [redir_kind], 2
    jne .sr_create
    mov ax, 0x3D01                 /* >>: append after seeking EOF */
.sr_create:
    xor cx, cx
    lea dx, [redir_name]
    int 0x21
    jc .sr_fail
    mov [redir_handle], bx
    cmp byte ptr [redir_kind], 2
    jne .sr_out_dup
    mov bx, [redir_handle]
    mov ax, 0x4202
    xor cx, cx
    xor dx, dx
    int 0x21
    jnc .sr_out_dup
    mov ax, 0x3C00                /* absent >> target: create it */
    xor cx, cx
    lea dx, [redir_name]
    int 0x21
    jc .sr_fail
    mov [redir_handle], bx
.sr_out_dup:
    mov bx, 1
    mov ah, 0x45
    int 0x21
    jc .sr_fail
    mov [saved_stdout], ax
    mov bx, [redir_handle]
    mov cx, 1
    mov ah, 0x46
    int 0x21
    clc
    ret
.sr_input:
    mov ax, 0x3D00
    lea dx, [redir_name]
    int 0x21
    jc .sr_fail
    mov [redir_handle], bx
    xor bx, bx
    mov ah, 0x45
    int 0x21
    jc .sr_fail
    mov [saved_stdin], ax
    mov bx, [redir_handle]
    xor cx, cx
    mov ah, 0x46
    int 0x21
.sr_ok:
    clc
    ret
.sr_fail:
    stc
    ret

restore_redirection:
    cmp byte ptr [redir_kind], 1
    je .rr_out
    cmp byte ptr [redir_kind], 2
    je .rr_out
    cmp byte ptr [redir_kind], 3
    jne .rr_done
    call restore_stdin
    jmp .rr_close
.rr_out:
    call restore_stdout
.rr_close:
    mov bx, [redir_handle]
    mov ah, 0x3E
    int 0x21
.rr_done:
    ret

restore_stdout:
    mov bx, [saved_stdout]
    mov cx, 1
    mov ah, 0x46
    int 0x21
    mov bx, [saved_stdout]
    mov ah, 0x3E
    int 0x21
    ret
restore_stdin:
    mov bx, [saved_stdin]
    xor cx, cx
    mov ah, 0x46
    int 0x21
    mov bx, [saved_stdin]
    mov ah, 0x3E
    int 0x21
    ret

/* Builtins and program execution, after shell syntax was stripped. */
dispatch_plain:
    lea si, [cmd]
    call skip_spaces
    cmp byte ptr [si], 0
    je .disp_ret

    lea di, [kw_if]
    call cmd_eq
    jnc .do_if
    lea di, [kw_set]
    call cmd_eq
    jnc .do_set
    lea di, [kw_pause]
    call cmd_eq
    jnc .do_pause
    lea di, [kw_ren]
    call cmd_eq
    jnc .do_ren
    lea di, [kw_rename]
    call cmd_eq
    jnc .do_ren
    lea di, [kw_ver]
    call cmd_eq
    jnc .do_ver
    lea di, [kw_call]
    call cmd_eq
    jnc .do_call
    lea di, [kw_goto]
    call cmd_eq
    jnc .do_goto
    lea di, [kw_echo]
    call cmd_eq
    jnc .do_echo
    lea di, [kw_dir]
    call cmd_eq
    jnc .do_dir
    lea di, [kw_type]
    call cmd_eq
    jnc .do_type
    lea di, [kw_copy]
    call cmd_eq
    jnc .do_copy
    lea di, [kw_del]
    call cmd_eq
    jnc .do_del
    lea di, [kw_cls]
    call cmd_eq
    jnc .do_cls
    lea di, [kw_cd]
    call cmd_eq
    jnc .do_cd
    lea di, [kw_chdir]
    call cmd_eq
    jnc .do_cd
    lea di, [kw_md]
    call cmd_eq
    jnc .do_md
    lea di, [kw_mkdir]
    call cmd_eq
    jnc .do_md
    lea di, [kw_rd]
    call cmd_eq
    jnc .do_rd
    lea di, [kw_rmdir]
    call cmd_eq
    jnc .do_rd

    call copy_token_to_prog
    call is_batch_name
    jnc .do_batch_file
    call build_exec_tail
    call try_exec
    jnc .disp_ret
    mov ah, 0x09
    lea dx, [msg_bad]
    int 0x21
.disp_ret:
    ret

.do_batch_file:
    call batch_run
    ret

.do_echo:
    call skip_token
    call skip_spaces
.echo_ch:
    lodsb
    test al, al
    jz .echo_nl
    mov [tch], al
    mov bx, 1
    mov cx, 1
    lea dx, [tch]
    mov ah, 0x40
    int 0x21
    jmp .echo_ch
.echo_nl:
    mov bx, 1
    mov cx, 2
    lea dx, [msg_crlf]
    mov ah, 0x40
    int 0x21
    ret

.do_if:
    call skip_token
    call skip_spaces
    lea di, [kw_errorlevel]
    call cmd_eq
    jc .if_exist
    call skip_token
    call skip_spaces
    call parse_u8
    mov ah, [last_errorlevel]
    cmp ah, al
    jb .disp_ret
    call skip_spaces
    lea di, [cmd]
.if_copy:
    lodsb
    stosb
    test al, al
    jnz .if_copy
    jmp dispatch

.if_exist:
    lea di, [kw_not]
    call cmd_eq
    jc .if_have_exist
    mov byte ptr [if_not], 1
    call skip_token
    call skip_spaces
    jmp .if_check_exist
.if_have_exist:
    mov byte ptr [if_not], 0
.if_check_exist:
    lea di, [kw_exist]
    call cmd_eq
    jc .disp_ret
    call skip_token
    call skip_spaces
    call copy_token_to_prog
    mov ax, 0x3D00
    lea dx, [prog]
    int 0x21
    mov al, 0
    jc .if_exists_done
    mov [if_open], bx
    mov ah, 0x3E
    int 0x21
    mov al, 1
.if_exists_done:
    xor al, [if_not]
    test al, al
    jz .disp_ret
    call skip_spaces
    lea di, [cmd]
.if_exist_copy:
    lodsb
    stosb
    test al, al
    jnz .if_exist_copy
    jmp dispatch

.do_pause:
    mov ah, 0x09
    lea dx, [msg_pause]
    int 0x21
    mov ah, 0x01
    int 0x21
    mov ah, 0x09
    lea dx, [msg_crlf]
    int 0x21
    ret

.do_ver:
    mov ah, 0x09
    lea dx, [msg_ver]
    int 0x21
    mov ah, 0x30
    int 0x21
    mov [ver_major], al
    mov [ver_minor], ah
    xor ah, ah
    xor dx, dx
    call print_u32
    mov dl, '.'
    mov ah, 0x02
    int 0x21
    mov al, [ver_minor]
    xor ah, ah
    xor dx, dx
    call print_u32
    mov ah, 0x09
    lea dx, [msg_crlf]
    int 0x21
    ret

.do_ren:
    call skip_token
    call skip_spaces
    call copy_token_to_prog
    mov [arg_after], si
    lea di, [srcbuf]
    lea si, [prog]
.ren_old:
    lodsb
    stosb
    test al, al
    jnz .ren_old
    mov si, [arg_after]
    call skip_spaces
    cmp byte ptr [si], 0
    je .ren_err
    call copy_token_to_prog
    lea di, [dstbuf]
    lea si, [prog]
.ren_new:
    lodsb
    stosb
    test al, al
    jnz .ren_new
    lea dx, [srcbuf]
    lea di, [dstbuf]
    push ds
    pop es
    mov ah, 0x56
    int 0x21
    jnc .disp_ret
.ren_err:
    mov ah, 0x09
    lea dx, [msg_ren_e]
    int 0x21
    ret

.do_call:
    call skip_token
    call skip_spaces
    cmp byte ptr [si], 0
    je .disp_ret
    call copy_token_to_prog
    call batch_run
    ret

.do_goto:
    call skip_token
    call skip_spaces
    call copy_token_to_prog
    lea si, [prog]
    lea di, [goto_target]
.gt_save:
    lodsb
    stosb
    test al, al
    jnz .gt_save
    /* Skip subsequent lines until :label (forward GOTO). */
    mov byte ptr [goto_active], 1
    ret

.do_set:
    call skip_token
    call skip_spaces
    cmp byte ptr [si], 0
    jne .set_assign
    call env_show
    ret
.set_assign:
    call env_set
    jc .set_err
    ret
.set_err:
    mov ah, 0x09
    lea dx, [msg_set_e]
    int 0x21
    ret

parse_u8:
    xor ax, ax
.pu8:
    mov bl, [si]
    cmp bl, '0'
    jb .pu8_d
    cmp bl, '9'
    ja .pu8_d
    inc si
    mov bh, 10
    mul bh
    sub bl, '0'
    add al, bl
    jmp .pu8
.pu8_d:
    ret

run_autoexec:
    lea si, [path_auto]
    lea di, [prog]
.rae_copy_path:
    lodsb
    stosb
    test al, al
    jnz .rae_copy_path
    lea si, [empty_args]
    call batch_run
    ret

/*
 * Synchronous batch runner.  The frame arrays are indexed by batch_depth;
 * CALL simply enters another frame and returns here when it reaches EOF.
 */
batch_run:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov [batch_arg_ptr], si
    cmp byte ptr [batch_depth], 4
    jae .rae_done
    lea dx, [prog]
    mov ax, 0x3D00
    int 0x21
    jc .rae_done
    xor ah, ah
    mov al, [batch_depth]
    shl ax, 1
    mov [batch_index], ax
    mov si, ax
    mov [batch_handles + si], bx
    mov di, [batch_name_base + si]
    lea si, [prog]
    call copy_asciz
    /* %0 is the invoked batch filename; remaining words are its arguments. */
    mov ax, [batch_index]
    mov si, ax
    mov bx, [batch_arg_base + si]
    lea di, [bx]
    lea si, [prog]
    call copy_asciz
    mov si, [batch_arg_ptr]
    add bx, 32
    mov cx, 9
.br_args:
    call skip_spaces
    cmp byte ptr [si], 0
    je .br_arg_zero
    push cx
    mov di, bx
    call copy_token_to_di
    pop cx
    add bx, 32
    loop .br_args
    jmp .br_args_done
.br_arg_zero:
    mov byte ptr [bx], 0
    add bx, 32
    loop .br_arg_zero
.br_args_done:
    inc byte ptr [batch_depth]
.rae_line:
    lea di, [cmd]
    xor cx, cx
.rae_byte:
    push cx
    mov ah, 0x3F
    xor bh, bh
    mov bl, [batch_depth]
    dec bl
    shl bx, 1
    mov bx, [batch_handles + bx]
    mov cx, 1
    lea dx, [tch]
    int 0x21
    pop cx
    jc .rae_close
    test ax, ax
    jz .rae_eof
    mov al, [tch]
    cmp al, 0x0D
    je .rae_byte
    cmp al, 0x0A
    je .rae_run
    cmp cx, 80
    jae .rae_byte
    mov [di], al
    inc di
    inc cx
    jmp .rae_byte
.rae_eof:
    test cx, cx
    jz .rae_close
.rae_run:
    mov byte ptr [di], 0
    test cx, cx
    jz .rae_line
    call batch_prepare_line
    jc .rae_line
    call dispatch
    jmp .rae_line
.rae_close:
    dec byte ptr [batch_depth]
    xor bh, bh
    mov bl, [batch_depth]
    shl bx, 1
    mov bx, [batch_handles + bx]
    mov ah, 0x3E
    int 0x21
.rae_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

show_prompt:
    push ax
    push dx
    push si
    mov ah, 0x19
    int 0x21
    add al, 'A'
    mov [prompt_drv], al
    mov ah, 0x09
    lea dx, [prompt_a]
    int 0x21
    lea si, [cwd_tmp]
    mov ah, 0x47
    mov dl, 0
    int 0x21
    lea si, [cwd_tmp]
.sp_c:
    lodsb
    test al, al
    jz .sp_done
    mov dl, al
    mov ah, 0x02
    int 0x21
    jmp .sp_c
.sp_done:
    mov ah, 0x09
    lea dx, [prompt_gt]
    int 0x21
    pop si
    pop dx
    pop ax
    ret

.do_cls:
    mov ax, 0x0003
    int 0x10
    ret

.do_cd:
    call skip_token
    call skip_spaces
    cmp byte ptr [si], 0
    je .cd_show
    call copy_token_to_prog
    mov ah, 0x3B
    lea dx, [prog]
    int 0x21
    jc .cd_err
    ret
.cd_show:
    lea si, [cwd_tmp]
    mov ah, 0x47
    mov dl, 0
    int 0x21
    mov ah, 0x09
    lea dx, [msg_a_colon]
    int 0x21
    lea si, [cwd_tmp]
.cd_p:
    lodsb
    test al, al
    jz .cd_nl
    mov dl, al
    mov ah, 0x02
    int 0x21
    jmp .cd_p
.cd_nl:
    mov ah, 0x09
    lea dx, [msg_crlf]
    int 0x21
    ret
.cd_err:
    mov ah, 0x09
    lea dx, [msg_cd_e]
    int 0x21
    ret

.do_md:
    call skip_token
    call skip_spaces
    cmp byte ptr [si], 0
    je .md_err
    call copy_token_to_prog
    mov ah, 0x39
    lea dx, [prog]
    int 0x21
    jc .md_err
    ret
.md_err:
    mov ah, 0x09
    lea dx, [msg_md_e]
    int 0x21
    ret

.do_rd:
    call skip_token
    call skip_spaces
    cmp byte ptr [si], 0
    je .rd_err
    call copy_token_to_prog
    mov ah, 0x3A
    lea dx, [prog]
    int 0x21
    jc .rd_err
    ret
.rd_err:
    mov ah, 0x09
    lea dx, [msg_rd_e]
    int 0x21
    ret

.do_dir:
    call skip_token
    call skip_spaces
    call dir_build_pattern
    mov word ptr [dir_count], 0
    mov word ptr [dir_bytes], 0
    mov word ptr [dir_bytes + 2], 0

    mov ah, 0x1A
    lea dx, [dta]
    int 0x21

    call dir_print_header

    mov ah, 0x4E
    lea dx, [dirpat]
    mov cx, 0x10                 /* include directories */
    int 0x21
    jc .dir_none
.dir_loop:
    call dir_print_entry
    mov ah, 0x4F
    int 0x21
    jnc .dir_loop
    call dir_print_footer
    ret
.dir_none:
    mov ah, 0x09
    lea dx, [msg_dir_nf]
    int 0x21
    ret

/*
 * Build CS:dirpat from command arg at SI.
 * empty → *.* ; wildcards or '.' → as-is ; else append \*.*
 */
dir_build_pattern:
    push ax
    push di
    lea di, [dirpat]
    cmp byte ptr [si], 0
    je .dbp_all
.dbp_copy:
    mov al, [si]
    test al, al
    jz .dbp_copied
    cmp al, ' '
    je .dbp_copied
    call up_al
    stosb
    inc si
    jmp .dbp_copy
.dbp_copied:
    mov byte ptr [di], 0
    lea di, [dirpat]
.dbp_scan:
    mov al, [di]
    test al, al
    jz .dbp_isdir
    cmp al, '*'
    je .dbp_done
    cmp al, '?'
    je .dbp_done
    cmp al, '.'
    je .dbp_done
    inc di
    jmp .dbp_scan
.dbp_isdir:
    mov byte ptr [di], '\\'
    inc di
    mov byte ptr [di], '*'
    inc di
    mov byte ptr [di], '.'
    inc di
    mov byte ptr [di], '*'
    inc di
    mov byte ptr [di], 0
    jmp .dbp_done
.dbp_all:
    mov byte ptr [di], '*'
    inc di
    mov byte ptr [di], '.'
    inc di
    mov byte ptr [di], '*'
    inc di
    mov byte ptr [di], 0
.dbp_done:
    pop di
    pop ax
    ret

/* Print " Directory of A:\" + cwd or parent of pattern */
dir_print_header:
    push ax
    push dx
    push si
    push di
    mov ah, 0x09
    lea dx, [msg_dir_hdr]
    int 0x21
    /* find last '\' in dirpat */
    lea si, [dirpat]
    xor di, di                   /* DI = offset of last '\', 0 if none */
.dph_scan:
    mov al, [si]
    test al, al
    jz .dph_scanned
    cmp al, '\\'
    jne .dph_n
    mov di, si
.dph_n:
    inc si
    jmp .dph_scan
.dph_scanned:
    test di, di
    jz .dph_cwd
    /* print chars from dirpat up to (not including) last '\' */
    lea si, [dirpat]
.dph_comp:
    cmp si, di
    jae .dph_nl
    mov dl, [si]
    mov ah, 0x02
    int 0x21
    inc si
    jmp .dph_comp
.dph_cwd:
    lea si, [cwd_tmp]
    mov ah, 0x47
    mov dl, 0
    int 0x21
    lea si, [cwd_tmp]
    cmp byte ptr [si], 0
    je .dph_nl
.dph_c:
    lodsb
    test al, al
    jz .dph_nl
    mov dl, al
    mov ah, 0x02
    int 0x21
    jmp .dph_c
.dph_nl:
    mov ah, 0x09
    lea dx, [msg_crlf]
    int 0x21
    pop di
    pop si
    pop dx
    pop ax
    ret

dir_print_entry:
    push ax
    push bx
    push cx
    push dx
    push si
    /* name padded to 12 columns */
    lea si, [dta + 0x1E]
    xor cx, cx
.dpe_name:
    lodsb
    test al, al
    jz .dpe_pad
    mov dl, al
    mov ah, 0x02
    int 0x21
    inc cx
    jmp .dpe_name
.dpe_pad:
    cmp cx, 13
    jae .dpe_attr
    mov dl, ' '
    mov ah, 0x02
    int 0x21
    inc cx
    jmp .dpe_pad
.dpe_attr:
    test byte ptr [dta + 0x15], 0x10
    jz .dpe_file
    mov ah, 0x09
    lea dx, [msg_dir_tag]
    int 0x21
    jmp .dpe_nl
.dpe_file:
    mov ax, [dta + 0x1A]
    mov dx, [dta + 0x1C]
    call print_u32
.dpe_nl:
    mov ah, 0x09
    lea dx, [msg_crlf]
    int 0x21
    /* count / bytes: skip . and .. */
    lea si, [dta + 0x1E]
    cmp byte ptr [si], '.'
    jne .dpe_count
    cmp byte ptr [si + 1], 0
    je .dpe_done
    cmp byte ptr [si + 1], '.'
    jne .dpe_count
    cmp byte ptr [si + 2], 0
    je .dpe_done
.dpe_count:
    inc word ptr [dir_count]
    test byte ptr [dta + 0x15], 0x10
    jnz .dpe_done
    mov ax, [dta + 0x1A]
    add [dir_bytes], ax
    mov ax, [dta + 0x1C]
    adc [dir_bytes + 2], ax
.dpe_done:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

dir_print_footer:
    push ax
    push bx
    push cx
    push dx
    mov ah, 0x09
    lea dx, [msg_dir_fs1]
    int 0x21
    mov ax, [dir_count]
    xor dx, dx
    call print_u32
    mov ah, 0x09
    lea dx, [msg_dir_fs2]
    int 0x21
    mov ax, [dir_bytes]
    mov dx, [dir_bytes + 2]
    call print_u32
    mov ah, 0x09
    lea dx, [msg_dir_fs3]
    int 0x21
    /* free_bytes = free_clusters * sectors_per_cluster * bytes_per_sector */
    mov ah, 0x36
    mov dl, 0
    int 0x21
    /* AX=spc, CX=bps, DX=free clusters */
    push dx                      /* free clusters */
    mul cx                       /* DX:AX = spc * bps (= bytes/cluster) */
    mov bx, ax                   /* BX = bpc low */
    mov cx, dx                   /* CX = bpc high (0 when spc=1,bps=512) */
    pop ax                       /* free clusters */
    /* DX:AX = clusters * bpc; bpc fits in BX when CX=0 (our image) */
    mul bx                       /* DX:AX = free * bpc_lo */
    call print_u32
    mov ah, 0x09
    lea dx, [msg_dir_fs4]
    int 0x21
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * Print DX:AX as unsigned decimal (no leading zeros except 0).
 */
print_u32:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    lea di, [numbuf + 10]
    mov byte ptr [di], 0
    mov bx, 10
    mov si, ax
    mov cx, dx                   /* CX:SI = value */
.pu_loop:
    /* divide CX:SI by 10 → quot in CX:SI, rem in DX */
    xor dx, dx
    mov ax, cx
    div bx
    mov cx, ax
    mov ax, si
    div bx
    mov si, ax
    add dl, '0'
    dec di
    mov [di], dl
    mov ax, cx
    or ax, si
    jnz .pu_loop
.pu_out:
    mov ah, 0x09
    /* convert ASCIZ at DI to $-string temporarily — print char by char */
.pu_ch:
    mov al, [di]
    test al, al
    jz .pu_done
    mov dl, al
    mov ah, 0x02
    int 0x21
    inc di
    jmp .pu_ch
.pu_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

.do_type:
    call skip_token
    call skip_spaces
    cmp byte ptr [si], 0
    je .type_usage
    call copy_token_to_prog
    mov ah, 0x3D
    xor al, al
    lea dx, [prog]
    int 0x21
    jc .type_err
    mov [th], bx
.tloop:
    mov ah, 0x3F
    mov bx, [th]
    mov cx, 1
    lea dx, [tch]
    int 0x21
    jc .tclose
    test ax, ax
    jz .tclose
    mov dl, [tch]
    mov ah, 0x02
    int 0x21
    jmp .tloop
.tclose:
    mov ah, 0x3E
    mov bx, [th]
    int 0x21
    ret
.type_usage:
    mov ah, 0x09
    lea dx, [msg_type_u]
    int 0x21
    ret
.type_err:
    mov ah, 0x09
    lea dx, [msg_type_e]
    int 0x21
    ret

.do_copy:
    call skip_token
    call skip_spaces
    call copy_token_to_prog
    push si
    lea si, [prog]
    lea di, [srcbuf]
.cp_s:
    lodsb
    stosb
    test al, al
    jnz .cp_s
    pop si
    call skip_spaces
    cmp byte ptr [si], 0
    je .copy_usage
    call copy_token_to_prog
    lea si, [prog]
    lea di, [dstbuf]
.cp_d:
    lodsb
    stosb
    test al, al
    jnz .cp_d

    mov ah, 0x3D
    xor al, al
    lea dx, [srcbuf]
    int 0x21
    jc .copy_err
    mov [th], bx
    mov ah, 0x3C
    xor cx, cx
    lea dx, [dstbuf]
    int 0x21
    jc .copy_err2
    mov [th2], bx
.cploop:
    mov ah, 0x3F
    mov bx, [th]
    mov cx, 128
    lea dx, [copybuf]
    int 0x21
    jc .cpdone
    test ax, ax
    jz .cpdone
    mov cx, ax
    mov ah, 0x40
    mov bx, [th2]
    lea dx, [copybuf]
    int 0x21
    jmp .cploop
.cpdone:
    mov ah, 0x3E
    mov bx, [th2]
    int 0x21
    mov ah, 0x3E
    mov bx, [th]
    int 0x21
    mov ah, 0x09
    lea dx, [msg_copied]
    int 0x21
    ret
.copy_err2:
    mov ah, 0x3E
    mov bx, [th]
    int 0x21
.copy_err:
.copy_usage:
    mov ah, 0x09
    lea dx, [msg_copy_e]
    int 0x21
    ret

.do_del:
    call skip_token
    call skip_spaces
    cmp byte ptr [si], 0
    je .del_usage
    call copy_token_to_prog
    mov ah, 0x41
    lea dx, [prog]
    int 0x21
    jc .del_err
    mov ah, 0x09
    lea dx, [msg_deleted]
    int 0x21
    ret
.del_usage:
.del_err:
    mov ah, 0x09
    lea dx, [msg_del_e]
    int 0x21
    ret

/*
 * After copy_token_to_prog, SI points past program name.
 * Build PSP-style tail at exec_tail and FCB stubs; fill exec_pb.
 */
build_exec_tail:
    push ax
    push bx
    push cx
    push si
    push di
    call skip_spaces
    lea di, [exec_tail + 1]
    xor cx, cx
.bet_c:
    mov al, [si]
    test al, al
    jz .bet_done
    cmp cx, 126
    jae .bet_done
    mov [di], al
    inc di
    inc si
    inc cx
    jmp .bet_c
.bet_done:
    mov byte ptr [di], 0x0D
    mov byte ptr [exec_tail], cl
    /* empty FCBs */
    lea di, [fcb1]
    mov cx, 32
    xor al, al
    rep stosb
    /* param block */
    mov ax, cs:[0x2C]                   /* preserve COMMAND's SET environment */
    mov word ptr [exec_pb], ax
    lea ax, [exec_tail]
    mov word ptr [exec_pb + 2], ax
    mov word ptr [exec_pb + 4], ds
    lea ax, [fcb1]
    mov word ptr [exec_pb + 6], ax
    mov word ptr [exec_pb + 8], ds
    lea ax, [fcb2]
    mov word ptr [exec_pb + 10], ax
    mov word ptr [exec_pb + 12], ds
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    ret

try_exec:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds
    pop es

    /* Remember the bare token before suffix mutation. */
    lea si, [prog]
    lea di, [prog_base]
    mov cx, 63
.te_save:
    lodsb
    stosb
    test al, al
    jz .te_saved
    loop .te_save
    mov byte ptr [di], 0
.te_saved:

    call try_exec_prog
    jnc .te_ok

    /* Paths with '\\' are absolute/relative — no PATH search. */
    lea si, [prog_base]
.te_slash:
    mov al, [si]
    test al, al
    jz .te_path
    cmp al, '\\'
    je .te_fail
    cmp al, '/'
    je .te_fail
    inc si
    jmp .te_slash

.te_path:
    call try_exec_on_path
    jnc .te_ok

.te_fail:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret
.te_ok:
    mov ah, 0x4D
    int 0x21
    mov [last_errorlevel], al
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret

/* Try DS:prog as-is, then .COM, then .EXE. CF clear on success. */
try_exec_prog:
    push ax
    push bx
    push dx
    lea bx, [exec_pb]
    lea dx, [prog]
    mov ax, 0x4B00
    int 0x21
    jnc .tep_ok
    call ensure_com_suffix
    lea bx, [exec_pb]
    lea dx, [prog]
    mov ax, 0x4B00
    int 0x21
    jnc .tep_ok
    call ensure_exe_suffix
    lea bx, [exec_pb]
    lea dx, [prog]
    mov ax, 0x4B00
    int 0x21
    jnc .tep_ok
    pop dx
    pop bx
    pop ax
    stc
    ret
.tep_ok:
    pop dx
    pop bx
    pop ax
    clc
    ret

/*
 * Walk PATH= from PSP environment; for each dir try dir\prog_base[.COM|.EXE].
 */
try_exec_on_path:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

    mov ax, cs:[0x2C]
    test ax, ax
    jz .teop_fail
    mov ds, ax
    xor si, si

.teop_find:
    cmp byte ptr [si], 0
    je .teop_fail
    mov al, [si]
    call up_al_cs
    cmp al, 'P'
    jne .teop_skipstr
    mov al, [si + 1]
    call up_al_cs
    cmp al, 'A'
    jne .teop_skipstr
    mov al, [si + 2]
    call up_al_cs
    cmp al, 'T'
    jne .teop_skipstr
    mov al, [si + 3]
    call up_al_cs
    cmp al, 'H'
    jne .teop_skipstr
    cmp byte ptr [si + 4], '='
    jne .teop_skipstr
    add si, 5
    jmp .teop_comp

.teop_skipstr:
    lodsb
    test al, al
    jnz .teop_skipstr
    jmp .teop_find

.teop_comp:
    /* DS:SI = next PATH component (env segment) */
    cmp byte ptr [si], 0
    je .teop_fail
    cmp byte ptr [si], ';'
    jne .teop_have
    inc si
    jmp .teop_comp

.teop_have:
    /* Copy component to CS:prog */
    push cs
    pop es
    lea di, [prog]
    mov cx, 48
.teop_cp:
    mov al, [si]
    test al, al
    jz .teop_cp_end
    cmp al, ';'
    je .teop_cp_semi
    mov es:[di], al
    inc di
    inc si
    loop .teop_cp
    jmp .teop_cp_end
.teop_cp_semi:
    inc si                          /* skip ';' — SI ready for next component */
.teop_cp_end:
    /* BX = SI for next component (env DS) */
    mov bx, si

    /* Empty component? */
    lea ax, [prog]
    cmp di, ax
    je .teop_next

    /* Append '\\' unless already a separator */
    mov al, es:[di - 1]
    cmp al, '\\'
    je .teop_addname
    cmp al, '/'
    je .teop_addname
    cmp al, ':'
    je .teop_addname
    mov byte ptr es:[di], '\\'
    inc di
.teop_addname:
    push ds
    push cs
    pop ds
    lea si, [prog_base]
.teop_an:
    lodsb
    stosb
    test al, al
    jnz .teop_an
    /* DS=CS; try exec */
    push bx
    call try_exec_prog
    pop bx
    pop ds                          /* restore env DS */
    jnc .teop_ok
.teop_next:
    mov si, bx
    jmp .teop_comp

.teop_ok:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.teop_fail:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

up_al_cs:
    cmp al, 'a'
    jb .uacs
    cmp al, 'z'
    ja .uacs
    sub al, 0x20
.uacs:
    ret

copy_asciz:
.ca:
    lodsb
    stosb
    test al, al
    jnz .ca
    ret

/* Copy one word from SI to DI, uppercasing it, and leave SI at its delimiter. */
copy_token_to_di:
.ctd:
    mov al, [si]
    test al, al
    jz .ctd_done
    cmp al, ' '
    je .ctd_done
    call up_al
    stosb
    inc si
    jmp .ctd
.ctd_done:
    mov byte ptr [di], 0
    ret

/* CF clear iff prog ends in .BAT (case insensitive; copy routine uppercases). */
is_batch_name:
    push si
    lea si, [prog]
.ibn_scan:
    cmp byte ptr [si], 0
    je .ibn_end
    inc si
    jmp .ibn_scan
.ibn_end:
    cmp si, offset prog + 4
    jb .ibn_no
    cmp byte ptr [si - 4], '.'
    jne .ibn_no
    cmp byte ptr [si - 3], 'B'
    jne .ibn_no
    cmp byte ptr [si - 2], 'A'
    jne .ibn_no
    cmp byte ptr [si - 1], 'T'
    jne .ibn_no
    pop si
    clc
    ret
.ibn_no:
    pop si
    stc
    ret

/*
 * Filter batch comments/labels and expand %n / %NAME%.  CF set means skip.
 * Expansion is deliberately bounded to cmd's 80-byte command-line limit.
 */
batch_prepare_line:
    lea si, [cmd]
    call skip_spaces
    cmp byte ptr [si], 0
    je .bpl_skip
    /* GOTO skip mode: ignore lines until matching :label */
    cmp byte ptr [goto_active], 0
    je .bpl_norm
    cmp byte ptr [si], ':'
    jne .bpl_skip
    inc si
    lea di, [prog_base]
    call copy_token_to_di
    lea si, [prog_base]
    lea di, [goto_target]
    call strings_equal
    jc .bpl_skip
    mov byte ptr [goto_active], 0
    jmp .bpl_skip
.bpl_norm:
    cmp byte ptr [si], '@'
    jne .bpl_noat
    inc si
    call skip_spaces
.bpl_noat:
    cmp byte ptr [si], ':'
    je .bpl_skip
    lea di, [kw_rem]
    call cmd_eq
    jnc .bpl_skip
    lea di, [expandbuf]
    xor cx, cx
.bpl_loop:
    lodsb
    test al, al
    jz .bpl_done
    cmp al, '%'
    jne .bpl_put
    mov al, [si]
    test al, al
    jz .bpl_put_pct
    cmp al, '0'
    jb .bpl_env
    cmp al, '9'
    ja .bpl_env
    sub al, '0'
    inc si
    call batch_arg_expand
    jmp .bpl_loop
.bpl_env:
    call env_expand
    jmp .bpl_loop
.bpl_put_pct:
    mov al, '%'
.bpl_put:
    cmp cx, 80
    jae .bpl_loop
    stosb
    inc cx
    jmp .bpl_loop
.bpl_done:
    mov byte ptr [di], 0
    lea si, [expandbuf]
    lea di, [cmd]
    push ds
    pop es
    call copy_asciz
    clc
    ret
.bpl_skip:
    stc
    ret

batch_arg_expand:
    push bx
    push si
    push di
    push cx
    xor ah, ah
    shl ax, 5
    mov bx, ax
    xor ah, ah
    mov al, [batch_depth]
    dec al
    shl ax, 1
    mov si, ax
    add bx, [batch_arg_base + si]
    mov si, bx
.bae_copy:
    lodsb
    test al, al
    jz .bae_done
    cmp cx, 80
    jae .bae_copy
    stosb
    inc cx
    jmp .bae_copy
.bae_done:
    pop cx
    pop di
    pop si
    pop bx
    ret

/* SI points just after first %, scans NAME% and appends its environment value. */
env_expand:
    push bx
    push dx
    push bp
    mov bp, si
    lea bx, [env_name]
    mov dx, bx
.ee_name:
    mov al, [si]
    test al, al
    jz .ee_literal
    cmp al, '%'
    je .ee_found
    cmp bx, offset env_name + 31
    jae .ee_literal
    mov [bx], al
    inc bx
    inc si
    jmp .ee_name
.ee_found:
    mov byte ptr [bx], 0
    inc si
    call env_find
    jc .ee_local
.ee_val:
    mov al, es:[bx]
    test al, al
    jz .envexp_done
    cmp cx, 80
    jae .ee_val_next
    mov [di], al
    inc di
    inc cx
.ee_val_next:
    inc bx
    jmp .ee_val
.ee_local:
    call last_set_expand
    jmp .envexp_done
.ee_literal:
    mov si, bp
    mov al, '%'
    cmp cx, 80
    jae .envexp_done
    stosb
    inc cx
.envexp_done:
    pop bp
    pop dx
    pop bx
    ret

/* Fallback cache also keeps expansion reliable if a legacy env block is full. */
last_set_expand:
    push si
    push bx
    lea si, [env_name]
    lea bx, [last_set]
.lse_cmp:
    mov al, [si]
    test al, al
    jz .lse_eq
    mov ah, [bx]
    call up_al
    call up_ah
    cmp al, ah
    jne .lse_done
    inc si
    inc bx
    jmp .lse_cmp
.lse_eq:
    cmp byte ptr [bx], '='
    jne .lse_done
    inc bx
.lse_copy:
    mov al, [bx]
    test al, al
    jz .lse_done
    cmp cx, 80
    jae .lse_next
    mov [di], al
    inc di
    inc cx
.lse_next:
    inc bx
    jmp .lse_copy
.lse_done:
    pop bx
    pop si
    ret

/* Find env_name in the COMMAND PSP environment.  ES:BX -> value, CF on miss. */
env_find:
    push ax
    push si
    push di
    mov ax, cs:[0x2C]
    mov es, ax
    xor bx, bx
.ef_entry:
    cmp byte ptr es:[bx], 0
    je .ef_miss
    lea si, [env_name]
    mov di, bx
.ef_cmp:
    mov al, [si]
    test al, al
    jz .ef_eq
    mov ah, es:[di]
    cmp ah, '='
    je .ef_next
    call up_al
    call up_ah
    cmp al, ah
    jne .ef_next
    inc si
    inc di
    jmp .ef_cmp
.ef_eq:
    cmp byte ptr es:[di], '='
    jne .ef_next
    lea bx, [di + 1]
    pop di
    pop si
    pop ax
    clc
    ret
.ef_next:
    inc bx
    cmp byte ptr es:[bx], 0
    jne .ef_next
    inc bx
    jmp .ef_entry
.ef_miss:
    pop di
    pop si
    pop ax
    stc
    ret

env_show:
    push ax
    push bx
    push dx
    push es
    mov ax, cs:[0x2C]
    mov es, ax
    xor bx, bx
.es_c:
    mov dl, es:[bx]
    test dl, dl
    jz .es_end
    mov ah, 0x02
    int 0x21
    inc bx
    jmp .es_c
.es_end:
    mov ah, 0x09
    lea dx, [msg_crlf]
    int 0x21
    inc bx
    cmp byte ptr es:[bx], 0
    jne .es_c
    pop es
    pop dx
    pop bx
    pop ax
    ret

/* Append NAME=VALUE at the environment double-NUL (up to byte 239). */
env_set:
    push ax
    push bx
    push dx
    push di
    push es
    push si
    lea di, [last_set]
.set_cache:
    lodsb
    stosb
    test al, al
    jnz .set_cache
    pop si
    mov ax, cs:[0x2C]
    mov es, ax
    xor bx, bx
.set_end:
    cmp byte ptr es:[bx], 0
    jne .set_next
    cmp byte ptr es:[bx + 1], 0
    je .set_here
.set_next:
    inc bx
    jmp .set_end
.set_here:
    mov di, bx
    mov dx, si
.set_copy:
    mov al, [si]
    test al, al
    jz .set_finish
    cmp di, 238
    jae .set_fail
    mov es:[di], al
    inc di
    inc si
    jmp .set_copy
.set_finish:
    mov byte ptr es:[di], 0
    mov byte ptr es:[di + 1], 0
    clc
    jmp .set_done
.set_fail:
    stc
.set_done:
    pop es
    pop di
    pop dx
    pop bx
    pop ax
    ret

/* Seek current frame to zero and scan for :label. */
batch_goto:
    cmp byte ptr [batch_depth], 0
    je .bg_done
    xor bh, bh
    mov bl, [batch_depth]
    dec bl
    shl bx, 1
    mov bx, [batch_handles + bx]
    mov ax, 0x4200
    xor cx, cx
    xor dx, dx
    int 0x21
.bg_line:
    call batch_read_line
    jc .bg_done
    lea si, [cmd]
    call skip_spaces
    cmp byte ptr [si], ':'
    jne .bg_line
    inc si
    lea di, [prog_base]
    call copy_token_to_di
    lea si, [prog_base]
    lea di, [goto_target]
    call strings_equal
    jnc .bg_done
    jmp .bg_line
.bg_done:
    ret

strings_equal:
    push ax
.seq_loop:
    mov al, [si]
    cmp al, [di]
    jne .seq_no
    test al, al
    jz .seq_yes
    inc si
    inc di
    jmp .seq_loop
.seq_no:
    pop ax
    stc
    ret
.seq_yes:
    pop ax
    clc
    ret

/* Read a raw line from the active batch frame. CF signals EOF. */
batch_read_line:
    lea di, [cmd]
    xor cx, cx
.brl_byte:
    push cx
    xor bh, bh
    mov bl, [batch_depth]
    dec bl
    shl bx, 1
    mov bx, [batch_handles + bx]
    mov ah, 0x3F
    mov cx, 1
    lea dx, [tch]
    int 0x21
    pop cx
    jc .brl_eof
    test ax, ax
    jz .brl_eof
    mov al, [tch]
    cmp al, 0x0D
    je .brl_byte
    cmp al, 0x0A
    je .brl_done
    cmp cx, 80
    jae .brl_byte
    mov [di], al
    inc di
    inc cx
    jmp .brl_byte
.brl_done:
    mov byte ptr [di], 0
    clc
    ret
.brl_eof:
    mov byte ptr [di], 0
    stc
    ret

skip_spaces:
.ss:
    cmp byte ptr [si], ' '
    jne .ss_d
    inc si
    jmp .ss
.ss_d:
    ret

skip_token:
.st:
    mov al, [si]
    test al, al
    jz .st_d
    cmp al, ' '
    je .st_d
    inc si
    jmp .st
.st_d:
    ret

cmd_eq:
    push ax
    push si
    push di
.ce:
    mov al, [di]
    test al, al
    jz .ce_endword
    mov ah, [si]
    call up_ah
    call up_al
    cmp al, ah
    jne .ce_no
    inc si
    inc di
    jmp .ce
.ce_endword:
    mov al, [si]
    test al, al
    jz .ce_yes
    cmp al, ' '
    je .ce_yes
.ce_no:
    pop di
    pop si
    pop ax
    stc
    ret
.ce_yes:
    pop di
    pop si
    pop ax
    clc
    ret

up_al:
    cmp al, 'a'
    jb .ua
    cmp al, 'z'
    ja .ua
    sub al, 0x20
.ua:
    ret
up_ah:
    cmp ah, 'a'
    jb .uh
    cmp ah, 'z'
    ja .uh
    sub ah, 0x20
.uh:
    ret

copy_token_to_prog:
    push ax
    push di
    lea di, [prog]
.ct:
    mov al, [si]
    test al, al
    jz .ct_d
    cmp al, ' '
    je .ct_d
    call up_al
    stosb
    inc si
    jmp .ct
.ct_d:
    mov byte ptr [di], 0
    pop di
    pop ax
    ret

ensure_com_suffix:
    push ax
    push si
    lea si, [prog]
.ec:
    cmp byte ptr [si], 0
    je .ec_add
    cmp byte ptr [si], '.'
    je .ec_done
    inc si
    jmp .ec
.ec_add:
    mov byte ptr [si], '.'
    mov byte ptr [si + 1], 'C'
    mov byte ptr [si + 2], 'O'
    mov byte ptr [si + 3], 'M'
    mov byte ptr [si + 4], 0
.ec_done:
    pop si
    pop ax
    ret

ensure_exe_suffix:
    push ax
    push si
    lea si, [prog]
.ee:
    cmp byte ptr [si], 0
    je .ee_add
    cmp byte ptr [si], '.'
    je .ee_rep
    inc si
    jmp .ee
.ee_rep:
    mov byte ptr [si + 1], 'E'
    mov byte ptr [si + 2], 'X'
    mov byte ptr [si + 3], 'E'
    mov byte ptr [si + 4], 0
    jmp .ee_done
.ee_add:
    mov byte ptr [si], '.'
    mov byte ptr [si + 1], 'E'
    mov byte ptr [si + 2], 'X'
    mov byte ptr [si + 3], 'E'
    mov byte ptr [si + 4], 0
.ee_done:
    pop si
    pop ax
    ret

prompt_a:
prompt_drv:
    .byte 'A'
    .ascii ":$"
prompt_gt:
    .ascii "> $"
msg_a_colon:
    .ascii "A:\\$"
msg_crlf:
    .ascii "\r\n$"
kw_dir:
    .asciz "DIR"
kw_type:
    .asciz "TYPE"
kw_copy:
    .asciz "COPY"
kw_del:
    .asciz "DEL"
kw_cls:
    .asciz "CLS"
kw_cd:
    .asciz "CD"
kw_chdir:
    .asciz "CHDIR"
kw_md:
    .asciz "MD"
kw_mkdir:
    .asciz "MKDIR"
kw_rd:
    .asciz "RD"
kw_rmdir:
    .asciz "RMDIR"
kw_echo:
    .asciz "ECHO"
kw_if:
    .asciz "IF"
kw_errorlevel:
    .asciz "ERRORLEVEL"
kw_exist:
    .asciz "EXIST"
kw_not:
    .asciz "NOT"
kw_set:
    .asciz "SET"
kw_pause:
    .asciz "PAUSE"
kw_ren:
    .asciz "REN"
kw_rename:
    .asciz "RENAME"
kw_ver:
    .asciz "VER"
kw_call:
    .asciz "CALL"
kw_goto:
    .asciz "GOTO"
kw_rem:
    .asciz "REM"
path_auto:
    .asciz "AUTOEXEC.BAT"
path_pipe:
    .asciz "A:\\PIPE.$$$"
empty_args:
    .byte 0
msg_dir_hdr:
    .ascii " Directory of A:\\$"
msg_dir_tag:
    .ascii "<DIR>$"
msg_dir_fs1:
    .ascii "        $"
msg_dir_fs2:
    .ascii " File(s)     $"
msg_dir_fs3:
    .ascii " bytes\r\n                    $"
msg_dir_fs4:
    .ascii " bytes free\r\n$"
msg_dir_nf:
    .ascii "File not found\r\n$"
msg_type_u:
    .ascii "TYPE file\r\n$"
msg_type_e:
    .ascii "file not found\r\n$"
msg_copy_e:
    .ascii "COPY src dst\r\n$"
msg_copied:
    .ascii "copied\r\n$"
msg_del_e:
    .ascii "DEL file\r\n$"
msg_deleted:
    .ascii "deleted\r\n$"
msg_cd_e:
    .ascii "Invalid directory\r\n$"
msg_md_e:
    .ascii "Unable to create directory\r\n$"
msg_rd_e:
    .ascii "Invalid path, not directory,\r\nor directory not empty\r\n$"
msg_bad:
    .ascii "Bad command\r\n$"
msg_pause:
    .ascii "Press any key to continue . . .$"
msg_ren_e:
    .ascii "RENAME old new\r\n$"
msg_set_e:
    .ascii "SET: environment full\r\n$"
msg_ver:
    .ascii "rmDOS DOS $"
line:
    .space 82, 0
cmd:
    .space 82, 0
prog:
    .space 80, 0
prog_base:
    .space 64, 0
goto_target:
    .space 64, 0
goto_active:
    .byte 0
srcbuf:
    .space 64, 0
dstbuf:
    .space 64, 0
cwd_tmp:
    .space 64, 0
dirpat:
    .space 64, 0
numbuf:
    .space 12, 0
dir_count:
    .word 0
dir_bytes:
    .word 0, 0
exec_tail:
    .space 128, 0
exec_pb:
    .space 14, 0
fcb1:
    .space 16, 0
fcb2:
    .space 16, 0
copybuf:
    .space 128, 0
dta:
    .space 128, 0
th:
    .word 0
th2:
    .word 0
ahandle:
    .word 0
batch_handles:
    .word 0, 0, 0, 0
batch_name_base:
    .word batch_name0, batch_name1, batch_name2, batch_name3
batch_arg_base:
    .word batch_args0, batch_args1, batch_args2, batch_args3
batch_depth:
    .byte 0
batch_index:
    .word 0
batch_arg_ptr:
    .word 0
arg_after:
    .word 0
batch_name0:
    .space 64, 0
batch_name1:
    .space 64, 0
batch_name2:
    .space 64, 0
batch_name3:
    .space 64, 0
batch_args0:
    .space 320, 0
batch_args1:
    .space 320, 0
batch_args2:
    .space 320, 0
batch_args3:
    .space 320, 0
expandbuf:
    .space 82, 0
pipe_rhs:
    .space 82, 0
redir_name:
    .space 64, 0
env_name:
    .space 32, 0
last_set:
    .space 80, 0
redir_kind:
    .byte 0
redir_handle:
    .word 0
saved_stdin:
    .word 0
saved_stdout:
    .word 0
if_not:
    .byte 0
if_open:
    .word 0
ver_major:
    .byte 0
ver_minor:
    .byte 0
tch:
    .byte 0
last_errorlevel:
    .byte 0
