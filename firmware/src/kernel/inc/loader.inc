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
.equ EXE_HDR_MAX, 0x800
.equ COM_LOAD_MAX, 0xFE00
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

    call exec_load_image
    jc .lrc_fail

    push cs
    pop ds

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
 * Open path_seg:path_off, stream COM/MZ into an AH=48 block, close handle.
 * OUT: CF set on failure (com_err set). psp_run / exe_* filled on success.
 */
exec_load_image:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds

    mov ds, word ptr cs:[path_seg]
    mov dx, word ptr cs:[path_off]
    mov ah, 0x3D
    xor al, al
    int 0x21
    jnc .eli_opened
    mov byte ptr cs:[com_err], 1
    jmp .eli_fail
.eli_opened:
    push cs
    pop ds
    mov word ptr [com_handle], bx

    mov ah, 0x3F
    mov bx, word ptr [com_handle]
    mov cx, EXE_HDR_MAX
    lea dx, [exe_hdr]
    int 0x21
    jnc .eli_hdr_ok
    mov byte ptr [com_err], 2
    jmp .eli_fail_close
.eli_hdr_ok:
    mov word ptr [exe_hdr_len], ax
    test ax, ax
    jnz .eli_got_hdr
    mov byte ptr [com_err], 3
    jmp .eli_fail_close
.eli_got_hdr:

    cmp word ptr [exe_hdr], 0x5A4D
    je .eli_exe
    cmp word ptr [exe_hdr], 0x4D5A
    je .eli_exe
    call load_setup_com
    jmp .eli_after_setup
.eli_exe:
    call load_setup_exe
.eli_after_setup:
    pushf
    mov ah, 0x3E
    mov bx, word ptr cs:[com_handle]
    int 0x21
    popf
    jc .eli_fail
    clc
    jmp .eli_done
.eli_fail_close:
    mov ah, 0x3E
    mov bx, word ptr cs:[com_handle]
    int 0x21
.eli_fail:
    stc
.eli_done:
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
 * Stream CS:img_bytes_lo/hi bytes from com_handle into ES:DI.
 * Advances ES when a segment fills. CF set on short read / I/O error.
 */
stream_file_to_esdi:
    push ax
    push bx
    push cx
    push dx
    push si
    push ds
.sfe_loop:
    mov ax, word ptr cs:[img_bytes_lo]
    mov dx, word ptr cs:[img_bytes_hi]
    mov bx, ax
    or bx, dx
    jz .sfe_ok

    /* chunk = min(0x8000, bytes left in ES segment from DI, remaining) */
    mov cx, 0x8000
    mov bx, di
    neg bx                       /* 10000h - DI when DI≠0; 0 when DI=0 */
    jz .sfe_full_seg
    cmp bx, cx
    jae .sfe_have_chunk
    mov cx, bx
    jmp .sfe_have_chunk
.sfe_full_seg:
    /* DI=0 → full 64K window; still cap at 32K per AH=3F */
    mov cx, 0x8000
.sfe_have_chunk:
    test dx, dx
    jnz .sfe_do_read
    cmp cx, ax
    jbe .sfe_do_read
    mov cx, ax
.sfe_do_read:
    push es
    pop ds
    mov dx, di
    mov bx, word ptr cs:[com_handle]
    mov ah, 0x3F
    int 0x21
    jc .sfe_fail
    cmp ax, cx
    jne .sfe_fail
    add di, ax
    pushf
    sub word ptr cs:[img_bytes_lo], ax
    sbb word ptr cs:[img_bytes_hi], 0
    popf
    jnc .sfe_loop                /* DI did not wrap */
    mov ax, es
    add ax, 0x1000               /* +64K */
    mov es, ax
    xor di, di
    jmp .sfe_loop
.sfe_ok:
    clc
    jmp .sfe_done
.sfe_fail:
    stc
.sfe_done:
    pop ds
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * File size via AH=42 mode 2 into DX:AX; restores position to 0.
 * CF set on seek error.
 */
exec_file_size:
    push bx
    push cx
    mov bx, word ptr cs:[com_handle]
    mov ax, 0x4202
    xor cx, cx
    xor dx, dx
    int 0x21
    jc .efs_fail
    push ax
    push dx
    mov bx, word ptr cs:[com_handle]
    mov ax, 0x4200
    xor cx, cx
    xor dx, dx
    int 0x21
    pop dx
    pop ax
.efs_fail:
    pop cx
    pop bx
    ret

/*
 * Setup COM: allocate PSP + image + 8KiB stack; stream from open handle.
 */
load_setup_com:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es

    call exec_file_size
    jc .lsc_fail
    test dx, dx
    jnz .lsc_fail                /* >64K */
    cmp ax, COM_LOAD_MAX
    ja .lsc_fail
    test ax, ax
    jz .lsc_fail
    mov word ptr cs:[com_size], ax
    mov word ptr cs:[img_bytes_lo], ax
    mov word ptr cs:[img_bytes_hi], 0

    /* paragraphs = 0x10 (PSP) + ceil(com_size/16) + 0x200 (8K stack) */
    add ax, 15
    mov cl, 4
    shr ax, cl
    add ax, 0x10
    add ax, 0x200
    mov bx, ax
    mov ah, 0x48
    int 0x21
    jnc .lsc_got
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

    push ax
    dec ax
    mov es, ax
    mov bx, es:[3]
    pop ax
    mov cx, bx
    add bx, ax
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

    mov bx, cx
    /* COM SS is the PSP segment: SP must stay inside 64KiB (max paras 0x1000).
     * Large images request PSP+image+8K (>64K paragraphs); shl would wrap SP
     * into low memory and smash the program (desk.com ~60KiB hit this). */
    cmp bx, 0x1000
    jbe .lsc_sp_ok
    mov bx, 0x1000
.lsc_sp_ok:
    mov cl, 4
    shl bx, cl
    sub bx, 2
    mov word ptr [exe_sp], bx
    mov ax, word ptr [psp_run]
    mov word ptr [exe_ss], ax

    /* Seek to SOF and stream into PSP:0100 */
    mov bx, word ptr cs:[com_handle]
    mov ax, 0x4200
    xor cx, cx
    xor dx, dx
    int 0x21
    jc .lsc_fail_free
    mov es, word ptr [psp_run]
    mov di, 0x0100
    call stream_file_to_esdi
    jc .lsc_fail_free
    clc
    jmp .lsc_done
.lsc_fail_free:
    mov es, word ptr cs:[psp_run]
    mov ah, 0x49
    int 0x21
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
 * Setup MZ EXE: parse exe_hdr, alloc, seek past header, stream image, relocate.
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

    lea si, [exe_hdr]
    /* header bytes = header_paras << 4 */
    mov ax, cs:[si + 8]
    mov cl, 4
    shl ax, cl
    mov bp, ax                   /* BP = header bytes */
    cmp ax, EXE_HDR_MAX
    ja .lse_fail                 /* header (w/ relocs) must fit scratch */
    cmp ax, word ptr cs:[exe_hdr_len]
    ja .lse_fail                 /* short header read */

    /* file image bytes = pages*512 - (last? 512-last : 0)  — use DX:AX */
    mov ax, cs:[si + 4]          /* pages */
    mov bx, 512
    mul bx                       /* DX:AX = pages*512 */
    mov bx, cs:[si + 2]          /* bytes in last page */
    test bx, bx
    jz .lse_full
    sub ax, 512
    sbb dx, 0
    add ax, bx
    adc dx, 0
.lse_full:
    /* image bytes = file_image - header */
    sub ax, bp
    sbb dx, 0
    jc .lse_fail
    mov word ptr cs:[img_bytes_lo], ax
    mov word ptr cs:[img_bytes_hi], dx
    mov word ptr cs:[com_size], ax   /* low word for legacy readers */

    /* base_paras = ceil(image_bytes/16) + 16 */
    push ax
    push dx
    add ax, 15
    adc dx, 0
    /* DX:AX >> 4 → image paragraphs in AX (Elite ~4629 paras; fits 16-bit) */
    mov cx, 4
.lse_shr4:
    shr dx, 1
    rcr ax, 1
    loop .lse_shr4
    test dx, dx
    jnz .lse_fail_pop            /* image paras > 64K (impossible for real-mode EXE) */
    add ax, 16
    jc .lse_fail_pop
    mov bx, ax                   /* BX = base paras */
    mov word ptr cs:[saved_psp_tmp], ax  /* reuse as base scratch */
    pop dx
    pop ax
    mov dx, bx                   /* DX = base paras */

    mov ax, cs:[si + 0x0C]       /* maxalloc */
    cmp ax, 0xFFFF
    je .lse_want_all
    add bx, ax
    jc .lse_want_all
    jmp .lse_try_alloc
.lse_want_all:
    mov bx, 0xFFFF
.lse_try_alloc:
    mov ah, 0x48
    int 0x21
    jnc .lse_got_mem
    mov ax, cs:[si + 0x0A]       /* minalloc */
    add ax, dx
    jc .lse_fail
    cmp bx, ax
    jb .lse_fail
    mov ah, 0x48
    int 0x21
    jc .lse_fail
.lse_got_mem:
    mov word ptr [psp_run], ax

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

    /* Seek to load image and stream to PSP+10:0000 */
    mov bx, word ptr cs:[com_handle]
    mov ax, 0x4200
    xor cx, cx
    mov dx, bp                   /* header bytes — fits in 16-bit for our cap */
    int 0x21
    jc .lse_fail_free

    mov ax, word ptr [psp_run]
    add ax, 0x10
    mov es, ax
    xor di, di
    call stream_file_to_esdi
    jc .lse_fail_free

    /* Relocations from exe_hdr */
    push cs
    pop ds
    lea si, [exe_hdr]
    mov cx, cs:[si + 6]
    mov bx, cs:[si + 0x18]
    lea si, [exe_hdr]
    add si, bx
    mov dx, es                   /* load segment = PSP+10 — but ES may have advanced!
                                  * Reload load seg from psp_run+10. */
    mov ax, word ptr cs:[psp_run]
    add ax, 0x10
    mov dx, ax
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

    lea si, [exe_hdr]
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
.lse_fail_pop:
    pop dx
    pop ax
    jmp .lse_fail
.lse_fail_free:
    mov es, word ptr cs:[psp_run]
    mov ah, 0x49
    int 0x21
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

/*
 * Fill a PSP Job File Table sized to CS:max_handles (5..64).
 * IN: ES = target PSP. A valid parent contributes its far JFT; otherwise use
 * the classic CON/CON/CON/AUX/PRN indices and FFh padding. Tables over 20
 * entries live in their own AH=48 block.
 */
psp_fill_jft:
    push bp
    mov bp, sp
    sub sp, 8                    /* PSP, count, destination off/seg */
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

    mov ax, es
    mov word ptr [bp - 2], ax
    mov cx, word ptr cs:[max_handles]
    cmp cx, 5
    jae .pfj_count_lo
    mov cx, 5
.pfj_count_lo:
    cmp cx, MAX_HANDLES_CAP
    jbe .pfj_count_hi
    mov cx, MAX_HANDLES_CAP
.pfj_count_hi:
    mov word ptr [bp - 4], cx
    cmp cx, 20
    jbe .pfj_inline

    mov bx, cx
    add bx, 15
    mov cl, 4
    shr bx, cl
    mov ah, 0x48
    int 0x21
    jc .pfj_alloc_fallback
    mov word ptr [bp - 6], 0
    mov word ptr [bp - 8], ax
    jmp .pfj_set_fields

.pfj_alloc_fallback:
    /* Keep the PSP usable if the tiny auxiliary allocation cannot be made. */
    mov word ptr [bp - 4], 20
.pfj_inline:
    mov word ptr [bp - 6], 0x18
    mov ax, word ptr [bp - 2]
    mov word ptr [bp - 8], ax

.pfj_set_fields:
    mov es, word ptr [bp - 2]
    mov ax, word ptr [bp - 4]
    mov word ptr es:[0x32], ax
    mov ax, word ptr [bp - 6]
    mov word ptr es:[0x34], ax
    mov ax, word ptr [bp - 8]
    mov word ptr es:[0x36], ax

    mov es, word ptr [bp - 8]
    mov di, word ptr [bp - 6]
    mov cx, word ptr [bp - 4]
    mov al, 0xFF
    cld
    rep stosb

    mov ax, word ptr cs:[current_psp]
    test ax, ax
    jz .pfj_defaults
    cmp ax, word ptr [bp - 2]
    je .pfj_defaults
    mov ds, ax
    cmp word ptr [0], 0x20CD
    jne .pfj_defaults
    mov cx, word ptr [0x32]
    test cx, cx
    jz .pfj_defaults
    cmp cx, MAX_HANDLES_CAP
    jbe .pfj_parent_hi
    mov cx, MAX_HANDLES_CAP
.pfj_parent_hi:
    cmp cx, word ptr [bp - 4]
    jbe .pfj_parent_count
    mov cx, word ptr [bp - 4]
.pfj_parent_count:
    mov si, word ptr [0x34]
    mov ax, word ptr [0x36]
    test ax, ax
    jz .pfj_defaults
    mov ds, ax
    mov es, word ptr [bp - 8]
    mov di, word ptr [bp - 6]
    rep movsb
    jmp .pfj_done

.pfj_defaults:
    mov es, word ptr [bp - 8]
    mov di, word ptr [bp - 6]
    call psp_jft_defaults

.pfj_done:
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

/* IN: ES:DI = JFT destination. */
psp_jft_defaults:
    mov byte ptr es:[di], 0x01
    mov byte ptr es:[di + 1], 0x01
    mov byte ptr es:[di + 2], 0x01
    mov byte ptr es:[di + 3], 0x00
    mov byte ptr es:[di + 4], 0x02
    ret

/*
 * Resize the current PSP's JFT to BX entries. Invalid kernel pseudo-PSPs are
 * ignored. A new external block is committed before the old one is released.
 * OUT: CF set only when an external allocation fails. Preserves registers.
 */
psp_resize_current_jft:
    push bp
    mov bp, sp
    sub sp, 14                   /* PSP, new count, old count/off/seg/ext, new seg */
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

    cmp bx, 5
    jae .prj_count_lo
    mov bx, 5
.prj_count_lo:
    cmp bx, MAX_HANDLES_CAP
    jbe .prj_count_hi
    mov bx, MAX_HANDLES_CAP
.prj_count_hi:
    mov word ptr [bp - 4], bx

    mov ax, word ptr cs:[current_psp]
    test ax, ax
    jz .prj_ok
    mov word ptr [bp - 2], ax
    mov es, ax
    cmp word ptr es:[0], 0x20CD
    jne .prj_ok

    mov ax, word ptr es:[0x32]
    cmp ax, MAX_HANDLES_CAP
    jbe .prj_old_count
    mov ax, MAX_HANDLES_CAP
.prj_old_count:
    mov word ptr [bp - 6], ax
    mov ax, word ptr es:[0x34]
    mov word ptr [bp - 8], ax
    mov ax, word ptr es:[0x36]
    mov word ptr [bp - 10], ax
    mov word ptr [bp - 12], 0
    cmp word ptr [bp - 8], 0x18
    jne .prj_old_external
    cmp ax, word ptr [bp - 2]
    je .prj_old_ready
.prj_old_external:
    test ax, ax
    jz .prj_old_ready
    mov word ptr [bp - 12], 1
.prj_old_ready:
    cmp word ptr [bp - 4], 20
    jbe .prj_to_inline

    mov bx, word ptr [bp - 4]
    add bx, 15
    mov cl, 4
    shr bx, cl
    mov ah, 0x48
    int 0x21
    jc .prj_fail
    mov word ptr [bp - 14], ax

    mov es, ax
    xor di, di
    mov cx, word ptr [bp - 4]
    mov al, 0xFF
    cld
    rep stosb

    mov cx, word ptr [bp - 6]
    test cx, cx
    jz .prj_external_defaults
    cmp cx, word ptr [bp - 4]
    jbe .prj_external_count
    mov cx, word ptr [bp - 4]
.prj_external_count:
    mov ax, word ptr [bp - 10]
    test ax, ax
    jz .prj_external_defaults
    mov ds, ax
    mov si, word ptr [bp - 8]
    mov es, word ptr [bp - 14]
    xor di, di
    rep movsb
    jmp .prj_external_commit
.prj_external_defaults:
    mov es, word ptr [bp - 14]
    xor di, di
    call psp_jft_defaults

.prj_external_commit:
    mov es, word ptr [bp - 2]
    mov ax, word ptr [bp - 4]
    mov word ptr es:[0x32], ax
    mov word ptr es:[0x34], 0
    mov ax, word ptr [bp - 14]
    mov word ptr es:[0x36], ax
    jmp .prj_free_old

.prj_to_inline:
    cmp word ptr [bp - 12], 0
    je .prj_inline_existing

    mov es, word ptr [bp - 2]
    mov di, 0x18
    mov cx, 20
    mov al, 0xFF
    cld
    rep stosb
    mov cx, word ptr [bp - 6]
    cmp cx, 20
    jbe .prj_inline_count
    mov cx, 20
.prj_inline_count:
    test cx, cx
    jz .prj_inline_defaults
    mov ax, word ptr [bp - 10]
    test ax, ax
    jz .prj_inline_defaults
    mov ds, ax
    mov si, word ptr [bp - 8]
    mov es, word ptr [bp - 2]
    mov di, 0x18
    rep movsb
    jmp .prj_inline_commit

.prj_inline_existing:
    cmp word ptr [bp - 6], 0
    jne .prj_inline_commit
    mov es, word ptr [bp - 2]
    mov di, 0x18
    mov cx, 20
    mov al, 0xFF
    cld
    rep stosb
.prj_inline_defaults:
    mov es, word ptr [bp - 2]
    mov di, 0x18
    call psp_jft_defaults

.prj_inline_commit:
    mov es, word ptr [bp - 2]
    mov ax, word ptr [bp - 4]
    mov word ptr es:[0x32], ax
    mov word ptr es:[0x34], 0x18
    mov ax, word ptr [bp - 2]
    mov word ptr es:[0x36], ax

.prj_free_old:
    cmp word ptr [bp - 12], 0
    je .prj_ok
    mov ax, word ptr [bp - 10]
    test ax, ax
    jz .prj_ok
    mov es, ax
    mov ah, 0x49
    int 0x21

.prj_ok:
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
    clc
    ret
.prj_fail:
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
    stc
    ret

/* IN: ES = PSP. Free its external JFT, if any. Preserves registers and ES. */
psp_free_external_jft:
    push ax
    push bx
    push es
    mov bx, es
    cmp word ptr es:[0x34], 0x18
    jne .pfej_free
    cmp word ptr es:[0x36], bx
    je .pfej_done
.pfej_free:
    mov ax, word ptr es:[0x36]
    test ax, ax
    jz .pfej_done
    mov es, ax
    mov ah, 0x49
    int 0x21
.pfej_done:
    pop es
    pop bx
    pop ax
    ret

/*
 * Fill PSP fields shared by COM/EXE: parent, JFT, env, vectors, tail, FCBs.
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
    call psp_fill_jft

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
 * Create PSP at DX. AL≠0 → also set current_psp = DX (AH=26).
 * AH=55 leaves current_psp unchanged (AL=0).
 */
psp_create_at:
    push ax
    push bx
    push cx
    push si
    push di
    push es
    push ds

    mov es, dx
    xor di, di
    mov cx, 0x80
    xor ax, ax
    cld
    rep stosw

    mov byte ptr es:[0x00], 0xCD
    mov byte ptr es:[0x01], 0x20

    mov ax, dx
    dec ax
    push es
    mov es, ax
    mov al, es:[0]
    cmp al, 'M'
    je .pca_mcb
    cmp al, 'Z'
    je .pca_mcb
    pop es
    push es
    mov es, word ptr cs:[current_psp]
    mov ax, word ptr es:[0x02]
    pop es
    mov word ptr es:[0x02], ax
    jmp .pca_parent
.pca_mcb:
    mov bx, es:[3]
    pop es
    mov ax, dx
    add ax, bx
    mov word ptr es:[0x02], ax

.pca_parent:
    mov ax, word ptr cs:[current_psp]
    mov word ptr es:[0x16], ax
    call psp_fill_jft

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

    push es
    mov es, word ptr cs:[current_psp]
    mov ax, word ptr es:[0x2C]
    pop es
    mov word ptr es:[0x2C], ax

    mov byte ptr es:[0x80], 0
    mov byte ptr es:[0x81], 0x0D

    pop ds
    pop es
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    test al, al
    jz .pca_noset
    mov word ptr cs:[current_psp], dx
.pca_noset:
    ret

/* AH=26h — Create New PSP at DX; becomes current */
dos_ah26:
    mov al, 1
    call psp_create_at
    clc
    jmp int21_iret

/* AH=55h — Create Child PSP at DX; current unchanged */
dos_ah55:
    xor al, al
    call psp_create_at
    clc
    jmp int21_iret

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
    mov word ptr [tsr_psp], ax
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

    /*
     * Free on the dedicated kernel stack. Child SS may lie inside the PSP
     * arena (small-model MZ); parent SS must keep the EXEC frame untouched
     * until we return — no nested INT 21 from that frame either.
     */
    mov word ptr [term_depth_idx], bx
    mov ax, cs
    mov ss, ax
    lea sp, [kernel_stack_top]

    cmp byte ptr [com_depth], 0
    jne .crk_free
    mov byte ptr [com_active], 0
    mov word ptr [dta_seg], cs
    lea ax, [default_dta]
    mov word ptr [dta_off], ax
.crk_free:
    mov ax, word ptr [tsr_psp]
    mov es, ax
    call psp_free_external_jft_noint
    mov ax, word ptr es:[0x2C]
    test ax, ax
    jz .crk_no_env
    call mcb_free_seg
.crk_no_env:
    mov ax, word ptr [tsr_psp]
    call mcb_free_seg

    mov bx, word ptr [term_depth_idx]
    mov ss, word ptr [save_ss_tbl + bx]
    mov sp, word ptr [save_sp_tbl + bx]
    push cs
    pop es

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

/* AX = segment to free (owner MCB at AX-1). No INT 21. */
mcb_free_seg:
    push ax
    push es
    dec ax
    mov es, ax
    mov al, es:[0]
    cmp al, 'M'
    je .mfs_ok
    cmp al, 'Z'
    jne .mfs_done
.mfs_ok:
    mov word ptr es:[1], 0
    call mem_coalesce
.mfs_done:
    pop es
    pop ax
    ret

/* Like psp_free_external_jft but frees via mcb_free_seg (no nested INT 21). */
psp_free_external_jft_noint:
    push ax
    push bx
    push es
    mov bx, es
    cmp word ptr es:[0x34], 0x18
    jne .pfejn_free
    cmp word ptr es:[0x36], bx
    je .pfejn_done
.pfejn_free:
    mov ax, word ptr es:[0x36]
    test ax, ax
    jz .pfejn_done
    call mcb_free_seg
.pfejn_done:
    pop es
    pop bx
    pop ax
    ret

/* TSR: free env, keep PSP, shrink after switching to parent stack. */
com_tsr_kernel:
    cli
    mov ax, cs
    mov ds, ax
    mov es, ax

    mov word ptr [tsr_keep_paras], dx

    dec byte ptr [com_depth]
    xor bh, bh
    mov bl, [com_depth]
    shl bx, 1

    mov ax, word ptr [psp_tbl + bx]
    mov word ptr [tsr_psp], ax

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
    je .cts_psp_kernel
    mov ax, word ptr [psp_tbl + bx - 2]
    jmp .cts_psp_set
.cts_psp_kernel:
    mov ax, cs
.cts_psp_set:
    mov word ptr [current_psp], ax
    pop bx

    /* Free env on child stack; keep PSP resident. */
    push bx
    push es
    mov ax, word ptr [tsr_psp]
    mov es, ax
    mov ax, word ptr es:[0x2C]
    test ax, ax
    jz .cts_no_env
    mov es, ax
    mov ah, 0x49
    int 0x21
    mov es, word ptr [tsr_psp]
    mov word ptr es:[0x2C], 0
.cts_no_env:
    pop es
    pop bx

    push cs
    pop es
    mov ss, word ptr [save_ss_tbl + bx]
    mov sp, word ptr [save_sp_tbl + bx]
    cmp byte ptr [com_depth], 0
    jne .cts_shr
    mov byte ptr [com_active], 0
    mov word ptr [dta_seg], cs
    lea ax, [default_dta]
    mov word ptr [dta_off], ax
.cts_shr:
    push bx
    push es
    mov es, word ptr [tsr_psp]
    mov bx, word ptr [tsr_keep_paras]
    test bx, bx
    jnz .cts_do_shr
    /* DX=0: keep current MCB size */
    push es
    mov ax, es
    dec ax
    mov es, ax
    mov bx, word ptr es:[3]
    pop es
.cts_do_shr:
    mov ah, 0x4A
    int 0x21
    mov ax, es
    add ax, bx
    mov word ptr es:[0x02], ax
    pop es
    pop bx

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
    cmp al, 3
    je .a4b_overlay
    cmp al, 1
    je .a4b_load_noexec
    mov ax, 1
    stc
    jmp int21_iret_cf
.a4b_overlay:
    /* AL=3: load overlay at EPB segment; no PSP / no exec.
     * EPB: +0 WORD load segment, +2 WORD relocation factor
     * DS:DX = ASCIZ path (caller) */
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push es
    push ds
    mov word ptr cs:[path_off], dx
    mov word ptr cs:[path_seg], ds
    mov ax, word ptr es:[bx]
    mov word ptr cs:[ovl_load_seg], ax
    mov ax, word ptr es:[bx + 2]
    mov word ptr cs:[ovl_reloc], ax
    mov ds, word ptr cs:[path_seg]
    mov dx, word ptr cs:[path_off]
    mov ah, 0x3D
    xor al, al
    int 0x21
    jc .a4b_ovl_fail
    mov word ptr cs:[com_handle], bx
    push cs
    pop ds
    mov ah, 0x3F
    mov bx, word ptr [com_handle]
    mov cx, EXE_HDR_MAX
    lea dx, [exe_hdr]
    int 0x21
    jc .a4b_ovl_fail_cl
    mov word ptr [exe_hdr_len], ax
    test ax, ax
    jz .a4b_ovl_fail_cl

    cmp word ptr [exe_hdr], 0x5A4D
    je .a4b_ovl_mz
    cmp word ptr [exe_hdr], 0x4D5A
    je .a4b_ovl_mz

    /* COM/raw overlay: stream entire file to load seg */
    call exec_file_size
    jc .a4b_ovl_fail_cl
    mov word ptr cs:[img_bytes_lo], ax
    mov word ptr cs:[img_bytes_hi], dx
    mov bx, word ptr cs:[com_handle]
    mov ax, 0x4200
    xor cx, cx
    xor dx, dx
    int 0x21
    jc .a4b_ovl_fail_cl
    mov es, word ptr cs:[ovl_load_seg]
    xor di, di
    call stream_file_to_esdi
    jc .a4b_ovl_fail_cl
    jmp .a4b_ovl_close_ok

.a4b_ovl_mz:
    lea si, [exe_hdr]
    mov ax, cs:[si + 8]
    mov cl, 4
    shl ax, cl
    mov bp, ax
    cmp ax, EXE_HDR_MAX
    ja .a4b_ovl_fail_cl
    mov ax, cs:[si + 4]
    mov bx, 512
    mul bx
    mov bx, cs:[si + 2]
    test bx, bx
    jz .a4b_ovl_full
    sub ax, 512
    sbb dx, 0
    add ax, bx
    adc dx, 0
.a4b_ovl_full:
    sub ax, bp
    sbb dx, 0
    jc .a4b_ovl_fail_cl
    mov word ptr cs:[img_bytes_lo], ax
    mov word ptr cs:[img_bytes_hi], dx
    mov bx, word ptr cs:[com_handle]
    mov ax, 0x4200
    xor cx, cx
    mov dx, bp
    int 0x21
    jc .a4b_ovl_fail_cl
    mov es, word ptr cs:[ovl_load_seg]
    xor di, di
    call stream_file_to_esdi
    jc .a4b_ovl_fail_cl
    call overlay_apply_relocs
.a4b_ovl_close_ok:
    mov ah, 0x3E
    mov bx, word ptr cs:[com_handle]
    int 0x21
    pop ds
    pop es
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    xor ax, ax
    clc
    jmp int21_iret
.a4b_ovl_fail_cl:
    mov ah, 0x3E
    mov bx, word ptr cs:[com_handle]
    int 0x21
.a4b_ovl_fail:
    pop ds
    pop es
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    mov ax, 2
    stc
    jmp int21_iret_cf
.a4b_load_noexec:
    /* AL=1: load into memory, fill PSP, return CS:IP/SS:SP in EPB; do not run */
    mov word ptr cs:[exec_pb_seg], es
    mov word ptr cs:[exec_pb_off], bx
    mov byte ptr cs:[exec_pb_valid], 1
    mov word ptr cs:[path_off], dx
    mov word ptr cs:[path_seg], ds
    call exec_load_image
    jc .a4b_fail
.a4b_lne_fill:
    /* fill PSP; restore parent current_psp after EPB write */
    mov ax, word ptr cs:[current_psp]
    mov word ptr cs:[saved_psp_tmp], ax
    call psp_fill_common
    mov es, word ptr cs:[exec_pb_seg]
    mov bx, word ptr cs:[exec_pb_off]
    mov ax, word ptr cs:[exe_sp]
    mov word ptr es:[bx + 0x0E], ax
    mov ax, word ptr cs:[exe_ss]
    mov word ptr es:[bx + 0x10], ax
    mov ax, word ptr cs:[exe_ip]
    mov word ptr es:[bx + 0x12], ax
    mov ax, word ptr cs:[exe_cs]
    mov word ptr es:[bx + 0x14], ax
    mov ax, word ptr cs:[saved_psp_tmp]
    mov word ptr cs:[current_psp], ax
    mov byte ptr cs:[exec_pb_valid], 0
    xor ax, ax
    clc
    jmp int21_iret

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

/* Apply MZ relocations for overlay using ovl_reloc as segment delta. */
overlay_apply_relocs:
    push ax
    push bx
    push cx
    push dx
    push si
    push ds
    push es
    push cs
    pop ds
    mov bx, word ptr [exe_hdr + 0x18]   /* reloc table offset */
    mov cx, word ptr [exe_hdr + 0x06]   /* reloc count */
    jcxz .oar_done
    lea si, [exe_hdr]
    add si, bx
    mov dx, word ptr [ovl_reloc]
.oar_loop:
    mov ax, word ptr [si]
    mov bx, word ptr [si + 2]
    add bx, dx
    mov es, bx
    mov bx, ax
    add word ptr es:[bx], dx
    add si, 4
    loop .oar_loop
.oar_done:
    pop es
    pop ds
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret
