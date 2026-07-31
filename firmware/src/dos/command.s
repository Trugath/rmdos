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
 * Dispatch ASCIZ command in [cmd]. Returns after one command.
 */
dispatch:
    lea si, [cmd]
    call skip_spaces
    cmp byte ptr [si], 0
    je .disp_ret

    lea di, [kw_if]
    call cmd_eq
    jnc .do_if
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
    call build_exec_tail
    call try_exec
    jnc .disp_ret
    mov ah, 0x09
    lea dx, [msg_bad]
    int 0x21
.disp_ret:
    ret

.do_echo:
    call skip_token
    call skip_spaces
.echo_ch:
    lodsb
    test al, al
    jz .echo_nl
    mov dl, al
    mov ah, 0x02
    int 0x21
    jmp .echo_ch
.echo_nl:
    mov ah, 0x09
    lea dx, [msg_crlf]
    int 0x21
    ret

.do_if:
    call skip_token
    call skip_spaces
    lea di, [kw_errorlevel]
    call cmd_eq
    jc .disp_ret
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
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov ax, 0x3D00
    lea dx, [path_auto]
    int 0x21
    jc .rae_done
    mov [ahandle], bx
.rae_line:
    lea di, [cmd]
    xor cx, cx
.rae_byte:
    push cx
    mov ah, 0x3F
    mov bx, [ahandle]
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
    call dispatch
    jmp .rae_line
.rae_close:
    mov ah, 0x3E
    mov bx, [ahandle]
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
    mov ah, 0x1A
    lea dx, [dta]
    int 0x21
    mov ah, 0x4E
    lea dx, [pat_all]
    mov cx, 0x10                 /* include directories */
    int 0x21
    jc .dir_done
.dir_loop:
    lea si, [dta + 0x1E]
.dir_ch:
    lodsb
    test al, al
    jz .dir_nl
    mov dl, al
    mov ah, 0x02
    int 0x21
    jmp .dir_ch
.dir_nl:
    mov dl, 0x0D
    mov ah, 0x02
    int 0x21
    mov dl, 0x0A
    mov ah, 0x02
    int 0x21
    mov ah, 0x4F
    int 0x21
    jnc .dir_loop
.dir_done:
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
    mov word ptr [exec_pb], 0            /* env = inherit/build */
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
    .ascii "A:$"
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
path_auto:
    .asciz "AUTOEXEC.BAT"
pat_all:
    .asciz "*.*"
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
line:
    .space 82, 0
cmd:
    .space 82, 0
prog:
    .space 80, 0
prog_base:
    .space 64, 0
srcbuf:
    .space 64, 0
dstbuf:
    .space 64, 0
cwd_tmp:
    .space 64, 0
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
tch:
    .byte 0
last_errorlevel:
    .byte 0
