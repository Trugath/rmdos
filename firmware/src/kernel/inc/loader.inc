/*
 * PSP + load/run .COM / MZ .EXE via MCB arena.
 * Nesting via save_ss_tbl / save_sp_tbl (COM_NEST_MAX).
 *
 * AH=4Bh AL=0 parameter block at ES:BX:
 *   +00 WORD env segment (0 = inherit/build default)
 *   +02 DWORD command tail pointer (length byte + chars)
 *   +06 DWORD FCB1 pointer
 *   +0A DWORD FCB2 pointer
 */

.equ PSP_SIZE, 0x0100
.equ COM_MAX, 0x6000
.equ COM_NEST_MAX, 4

/*
 * IN: DS:DX = ASCIZ path.
 *     If exec_pb_valid≠0, CS:exec_pb_* holds caller's ES:BX param block.
 * OUT: CF on failure. Returns after child exits.
 */
load_and_run_com:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds

    mov word ptr cs:[path_off], dx
    mov word ptr cs:[path_seg], ds
    mov byte ptr cs:[com_err], 0

    cmp byte ptr cs:[com_depth], COM_NEST_MAX
    jb .lrc_depth_ok
    mov byte ptr cs:[com_err], 4
    jmp .lrc_fail
.lrc_depth_ok:

    mov ds, word ptr cs:[path_seg]
    mov dx, word ptr cs:[path_off]
    mov ah, 0x3D
    xor al, al
    int 0x21
    jnc .lrc_opened
    mov byte ptr cs:[com_err], 1
    jmp .lrc_fail
.lrc_opened:
    push cs
    pop ds
    mov word ptr [com_handle], bx

    mov ah, 0x3F
    mov bx, word ptr [com_handle]
    mov cx, COM_MAX
    lea dx, [com_buf]
    int 0x21
    jnc .lrc_read_ok
    mov byte ptr [com_err], 2
    jmp .lrc_fail_close
.lrc_read_ok:
    mov word ptr [com_size], ax
    test ax, ax
    jnz .lrc_got_bytes
    mov byte ptr [com_err], 3
    jmp .lrc_fail_close
.lrc_got_bytes:

    mov ah, 0x3E
    mov bx, word ptr [com_handle]
    int 0x21

    /* Detect MZ */
    cmp word ptr [com_buf], 0x5A4D
    je .lrc_exe
    cmp word ptr [com_buf], 0x4D5A
    je .lrc_exe
    jmp .lrc_com

.lrc_exe:
    call load_setup_exe
    jc .lrc_fail
    jmp .lrc_go

.lrc_com:
    call load_setup_com
    jc .lrc_fail

.lrc_go:
    call psp_fill_common
    /* default DTA = child PSP:0080 */
    mov ax, word ptr [psp_run]
    mov word ptr [dta_seg], ax
    mov word ptr [dta_off], 0x80

    /* save SS:SP and PSP */
    xor bh, bh
    mov bl, [com_depth]
    shl bx, 1
    mov ax, ss
    mov word ptr [save_ss_tbl + bx], ax
    mov ax, sp
    mov word ptr [save_sp_tbl + bx], ax
    mov ax, word ptr [psp_run]
    mov word ptr [psp_tbl + bx], ax
    inc byte ptr [com_depth]
    mov byte ptr [com_active], 1

    cli
    mov ax, word ptr [psp_run]
    mov ds, ax
    mov es, ax
    cmp word ptr cs:[exe_ss], 0
    je .lrc_com_stack
    mov ss, word ptr cs:[exe_ss]
    mov sp, word ptr cs:[exe_sp]
    jmp .lrc_stack_ok
.lrc_com_stack:
    mov ss, ax
    mov sp, 0xFFFE
.lrc_stack_ok:
    sti

    push word ptr cs:[exe_cs]
    push word ptr cs:[exe_ip]
    retf

.lrc_fail_close:
    mov ah, 0x3E
    mov bx, word ptr cs:[com_handle]
    int 0x21
.lrc_fail:
    mov byte ptr cs:[exec_pb_valid], 0
    stc
    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * Fill PSP fields shared by COM/EXE: parent, env, vectors, tail, FCBs.
 * ES = psp_run on entry not required; uses CS:psp_run.
 */
psp_fill_common:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds

    mov es, word ptr cs:[psp_run]

    /* INT 20h stub + mem top already set by setup */

    /* parent PSP */
    mov ax, word ptr cs:[current_psp]
    mov word ptr es:[0x16], ax

    /* save INT 22/23/24 into PSP */
    push ds
    xor ax, ax
    mov ds, ax
    mov ax, [0x22 * 4]
    mov word ptr es:[0x0A], ax
    mov ax, [0x22 * 4 + 2]
    mov word ptr es:[0x0C], ax
    mov ax, [0x23 * 4]
    mov word ptr es:[0x0E], ax
    mov ax, [0x23 * 4 + 2]
    mov word ptr es:[0x10], ax
    mov ax, [0x24 * 4]
    mov word ptr es:[0x12], ax
    mov ax, [0x24 * 4 + 2]
    mov word ptr es:[0x14], ax
    pop ds

    /* environment: use caller's env seg if non-zero, else build default */
    xor ax, ax
    cmp byte ptr cs:[exec_pb_valid], 0
    je .pfc_build_env
    push ds
    mov ds, word ptr cs:[exec_pb_seg]
    mov bx, word ptr cs:[exec_pb_off]
    mov ax, word ptr [bx]
    pop ds
    test ax, ax
    jnz .pfc_env_set
.pfc_build_env:
    call build_env_block
.pfc_env_set:
    mov word ptr es:[0x2C], ax

    /* command tail + FCBs from param block or empty */
    cmp byte ptr cs:[exec_pb_valid], 0
    je .pfc_empty_tail

    mov ds, word ptr cs:[exec_pb_seg]
    mov bx, word ptr cs:[exec_pb_off]
    /* DS:BX = param block */
    lds si, dword ptr [bx + 2]   /* command tail */
    mov cl, [si]
    mov ch, 0
    cmp cx, 127
    jbe .pfc_tail_ok
    mov cx, 127
.pfc_tail_ok:
    mov byte ptr es:[0x80], cl
    inc si
    mov di, 0x81
    jcxz .pfc_tail_z
    rep movsb
.pfc_tail_z:
    mov byte ptr es:[di], 0x0D

    /* FCB1 at 5Ch */
    mov ds, word ptr cs:[exec_pb_seg]
    mov bx, word ptr cs:[exec_pb_off]
    lds si, dword ptr [bx + 6]
    mov di, 0x5C
    mov cx, 16
    rep movsb
    /* FCB2 at 6Ch */
    mov ds, word ptr cs:[exec_pb_seg]
    mov bx, word ptr cs:[exec_pb_off]
    lds si, dword ptr [bx + 0x0A]
    mov di, 0x6C
    mov cx, 16
    rep movsb
    jmp .pfc_done

.pfc_empty_tail:
    mov byte ptr es:[0x80], 0
    mov byte ptr es:[0x81], 0x0D

.pfc_done:
    mov ax, word ptr cs:[psp_run]
    mov word ptr cs:[current_psp], ax
    mov byte ptr cs:[exec_pb_valid], 0
    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * Build minimal environment block.
 * OUT: AX = env segment (0 on failure — leave empty).
 */
build_env_block:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds

    /* ~16 paras for COMSPEC + PATH + exe pathname */
    mov bx, 16
    mov ah, 0x48
    int 0x21
    jc .beb_fail
    mov es, ax
    push cs
    pop ds
    xor di, di
    lea si, [env_comspec]
.beb_copy:
    lodsb
    stosb
    test al, al
    jnz .beb_copy
    lea si, [env_path]
.beb_path_var:
    lodsb
    stosb
    test al, al
    jnz .beb_path_var
    /* second NUL terminates env */
    stosb
    /* DOS 3+ appends pathname after double-NUL */
    mov ds, word ptr cs:[path_seg]
    mov si, word ptr cs:[path_off]
.beb_path:
    lodsb
    stosb
    test al, al
    jnz .beb_path
    mov ax, es
    jmp .beb_done
.beb_fail:
    xor ax, ax
.beb_done:
    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ret

/*
 * Setup COM: allocate PSP + image + 8KiB stack (not a full 64KiB arena) so
 * nested tools like FORMAT /S can still AH=48 system files. SP is set to the
 * top of the actual block.
 */
load_setup_com:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    /* paragraphs = 0x10 (PSP) + ceil(com_size/16) + 0x200 (8K stack) */
    mov ax, word ptr [com_size]
    add ax, 15
    mov cl, 4
    shr ax, cl
    add ax, 0x10
    add ax, 0x200
    mov bx, ax
    mov ah, 0x48
    int 0x21
    jnc .lsc_got
    /* Fall back to largest free block. */
    mov bx, 0xFFFF
    mov ah, 0x48
    int 0x21
    mov ah, 0x48
    int 0x21
    jc .lsc_fail
.lsc_got:
    mov word ptr [psp_run], ax
    mov word ptr [exe_cs], ax
    mov word ptr [exe_ip], 0x0100

    /* Read MCB size → mem top + SP */
    push ax
    dec ax
    mov es, ax
    mov bx, es:[3]
    pop ax
    mov cx, bx                   /* save paragraphs in CX */
    add bx, ax                   /* mem top paragraph */
    mov es, ax

    xor di, di
    push cx
    mov cx, PSP_SIZE
    xor al, al
    rep stosb
    pop cx
    mov byte ptr es:[0x00], 0xCD
    mov byte ptr es:[0x01], 0x20
    mov word ptr es:[0x02], bx

    /* SP = paragraphs*16 - 2 (always inside block; <= FFFE for <=64K) */
    mov bx, cx
    mov cl, 4
    shl bx, cl
    sub bx, 2
    mov word ptr [exe_sp], bx
    mov ax, word ptr [psp_run]
    mov word ptr [exe_ss], ax

    mov es, ax
    mov cx, word ptr [com_size]
    lea si, [com_buf]
    mov di, 0x0100
    push ds
    push cs
    pop ds
    rep movsb
    pop ds
    clc
    jmp .lsc_done
.lsc_fail:
    stc
.lsc_done:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * Setup MZ EXE from com_buf / com_size.
 */
load_setup_exe:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push bp

    lea si, [com_buf]
    mov ax, cs:[si + 8]
    mov cl, 4
    shl ax, cl
    mov bp, ax
    mov ax, cs:[si + 4]
    mov bx, 512
    mul bx
    mov bx, cs:[si + 2]
    test bx, bx
    jz .lse_full
    sub ax, 512
    add ax, bx
.lse_full:
    sub ax, bp
    mov word ptr [com_size], ax

    /* Base = image paragraphs + PSP (16). */
    mov bx, ax
    add bx, 15
    mov cl, 4
    shr bx, cl
    add bx, 16
    mov dx, bx                   /* DX = base paras */
    mov ax, cs:[si + 0x0C]       /* maxalloc */
    cmp ax, 0xFFFF
    je .lse_want_all
    add bx, ax                   /* base + maxalloc */
    jmp .lse_try_alloc
.lse_want_all:
    mov bx, 0xFFFF
.lse_try_alloc:
    mov ah, 0x48
    int 0x21
    jnc .lse_got_mem
    /* Largest free in BX; require base + minalloc. */
    mov ax, cs:[si + 0x0A]
    add ax, dx
    cmp bx, ax
    jb .lse_fail
    mov ah, 0x48
    int 0x21
    jc .lse_fail
.lse_got_mem:
    mov word ptr [psp_run], ax

    /* PSP mem top from actual MCB size */
    push ax
    dec ax
    mov es, ax
    mov bx, es:[3]
    pop ax
    add bx, ax

    mov es, ax
    xor di, di
    mov cx, PSP_SIZE
    xor al, al
    rep stosb
    mov byte ptr es:[0], 0xCD
    mov byte ptr es:[1], 0x20
    mov word ptr es:[2], bx

    mov ax, word ptr [psp_run]
    add ax, 0x10
    mov es, ax
    lea si, [com_buf]
    add si, bp
    xor di, di
    mov cx, word ptr [com_size]
    push ds
    push cs
    pop ds
    rep movsb
    pop ds

    push cs
    pop ds
    lea si, [com_buf]
    mov cx, cs:[si + 6]
    mov bx, cs:[si + 0x18]
    lea si, [com_buf]
    add si, bx
    mov dx, es
.lse_rel:
    jcxz .lse_rel_done
    mov di, cs:[si]
    mov ax, cs:[si + 2]
    add ax, dx
    push es
    mov es, ax
    add word ptr es:[di], dx
    pop es
    add si, 4
    dec cx
    jmp .lse_rel
.lse_rel_done:

    lea si, [com_buf]
    mov ax, cs:[si + 0x16]
    add ax, dx
    mov word ptr [exe_cs], ax
    mov ax, cs:[si + 0x14]
    mov word ptr [exe_ip], ax
    mov ax, cs:[si + 0x0E]
    add ax, dx
    mov word ptr [exe_ss], ax
    mov ax, cs:[si + 0x10]
    mov word ptr [exe_sp], ax
    clc
    jmp .lse_done
.lse_fail:
    stc
.lse_done:
    pop bp
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

com_resume_kernel:
    cli
    mov ax, cs
    mov ds, ax
    mov es, ax

    dec byte ptr [com_depth]
    xor bh, bh
    mov bl, [com_depth]
    shl bx, 1

    /* restore parent INT 22/23/24 from terminating PSP, then parent PSP */
    push bx
    mov ax, word ptr [psp_tbl + bx]
    push es
    push ds
    mov es, ax
    xor ax, ax
    mov ds, ax
    mov ax, word ptr es:[0x0A]
    mov word ptr [0x22 * 4], ax
    mov ax, word ptr es:[0x0C]
    mov word ptr [0x22 * 4 + 2], ax
    mov ax, word ptr es:[0x0E]
    mov word ptr [0x23 * 4], ax
    mov ax, word ptr es:[0x10]
    mov word ptr [0x23 * 4 + 2], ax
    mov ax, word ptr es:[0x12]
    mov word ptr [0x24 * 4], ax
    mov ax, word ptr es:[0x14]
    mov word ptr [0x24 * 4 + 2], ax
    pop ds
    pop es

    cmp bl, 0
    je .crk_psp_kernel
    mov ax, word ptr [psp_tbl + bx - 2]
    jmp .crk_psp_set
.crk_psp_kernel:
    mov ax, cs
.crk_psp_set:
    mov word ptr [current_psp], ax
    pop bx

    /* free this level's PSP block (also frees env if we tracked it — skip) */
    push bx
    mov ax, word ptr [psp_tbl + bx]
    /* free env block if non-zero */
    push es
    mov es, ax
    mov ax, word ptr es:[0x2C]
    test ax, ax
    jz .crk_no_env
    push bx
    mov es, ax
    mov ah, 0x49
    int 0x21
    pop bx
.crk_no_env:
    mov ax, word ptr [psp_tbl + bx]
    mov es, ax
    mov ah, 0x49
    int 0x21
    pop es
    pop bx

    push cs
    pop es
    mov ss, word ptr [save_ss_tbl + bx]
    mov sp, word ptr [save_sp_tbl + bx]
    cmp byte ptr [com_depth], 0
    jne .crk_nested
    mov byte ptr [com_active], 0
    /* restore kernel DTA */
    mov word ptr [dta_seg], cs
    lea ax, [default_dta]
    mov word ptr [dta_off], ax
.crk_nested:
    sti
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

dos_ah4b:
    cmp al, 0
    je .a4b_load
    mov ax, 1
    stc
    jmp int21_iret_cf
.a4b_load:
    /* save param block ES:BX */
    mov word ptr cs:[exec_pb_seg], es
    mov word ptr cs:[exec_pb_off], bx
    mov byte ptr cs:[exec_pb_valid], 1
    /* save parent DTA before child runs */
    xor bh, bh
    mov bl, byte ptr cs:[com_depth]
    shl bx, 1
    mov ax, word ptr cs:[dta_seg]
    mov word ptr cs:[save_dta_seg_tbl + bx], ax
    mov ax, word ptr cs:[dta_off]
    mov word ptr cs:[save_dta_off_tbl + bx], ax
    call load_and_run_com
    jc .a4b_fail
    /* restore parent DTA */
    xor bh, bh
    mov bl, byte ptr cs:[com_depth]
    shl bx, 1
    mov ax, word ptr cs:[save_dta_seg_tbl + bx]
    mov word ptr cs:[dta_seg], ax
    mov ax, word ptr cs:[save_dta_off_tbl + bx]
    mov word ptr cs:[dta_off], ax
    xor ax, ax
    clc
    jmp int21_iret
.a4b_fail:
    mov byte ptr cs:[exec_pb_valid], 0
    mov ax, 2
    stc
    jmp int21_iret_cf
