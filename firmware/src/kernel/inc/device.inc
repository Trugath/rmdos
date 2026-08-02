/*
 * Character-device SYS ABI — headers, strategy/interrupt call, DEVICE= loader.
 *
 * Header: +00 next far, +04 attrs, +06 strategy, +08 interrupt, +0A name[8]
 * Request: +00 len, +01 unit, +02 cmd, +03 status; OUTPUT +0D xfer far, +11 count
 * INIT cmd0: driver writes break far at +0E.
 */

.equ DEV_ATTR_CHAR, 0x8000
.equ DEV_ATTR_STDIN, 0x0001
.equ DEV_ATTR_STDOUT, 0x0002
.equ DEV_CMD_INIT, 0
.equ DEV_CMD_INPUT, 4
.equ DEV_CMD_INSTAT, 5
.equ DEV_CMD_OUTPUT, 8
.equ DEV_CMD_OUTSTAT, 10
.equ DEV_STAT_DONE, 0x0100
.equ DEV_REQ_SIZE, 22
.equ SYS_MAX, 0x2000

/* Boot: link NUL → CON, set con_dev = CON. */
dev_init_chain:
    push ax
    push bx
    mov ax, cs
    lea bx, [dev_con_hdr]
    mov word ptr cs:[dev_nul_hdr], bx
    mov word ptr cs:[dev_nul_hdr + 2], ax
    mov word ptr cs:[dev_con_hdr], 0xFFFF
    mov word ptr cs:[dev_con_hdr + 2], 0xFFFF
    lea bx, [dev_nul_hdr]
    mov word ptr cs:[dev_chain_off], bx
    mov word ptr cs:[dev_chain_seg], ax
    lea bx, [dev_con_hdr]
    mov word ptr cs:[con_dev_off], bx
    mov word ptr cs:[con_dev_seg], ax
    /* AH=32 DPB device header → builtin CON */
    mov word ptr cs:[dos_dpb + 0x13], bx
    mov word ptr cs:[dos_dpb + 0x15], ax
    pop bx
    pop ax
    ret

/*
 * Builtin CON — teletype + COM1 (former putch body).
 * Strategy saves ES:BX; interrupt handles INIT/OUTPUT/OUTSTAT.
 */
dev_con_strategy:
    mov word ptr cs:[dev_con_rh_off], bx
    mov word ptr cs:[dev_con_rh_seg], es
    retf

dev_con_interrupt:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    mov es, word ptr cs:[dev_con_rh_seg]
    mov bx, word ptr cs:[dev_con_rh_off]
    mov al, es:[bx + 2]
    cmp al, DEV_CMD_INIT
    je .dci_init
    cmp al, DEV_CMD_INPUT
    je .dci_in
    cmp al, DEV_CMD_INSTAT
    je .dci_istat
    cmp al, DEV_CMD_OUTPUT
    je .dci_out
    cmp al, DEV_CMD_OUTSTAT
    je .dci_ostat
    mov word ptr es:[bx + 3], DEV_STAT_DONE
    jmp .dci_done
.dci_init:
    /* break = end of this header block (no extra resident data) */
    lea ax, [dev_con_hdr_end]
    mov es:[bx + 0x0E], ax
    mov es:[bx + 0x10], cs
    mov word ptr es:[bx + 3], DEV_STAT_DONE
    jmp .dci_done
.dci_istat:
    mov ah, 0x01
    int 0x16
    mov ax, DEV_STAT_DONE
    jz .dci_istat_busy
    mov word ptr es:[bx + 3], ax
    jmp .dci_done
.dci_istat_busy:
    or ax, 0x0200               /* busy */
    mov word ptr es:[bx + 3], ax
    jmp .dci_done
.dci_in:
    push ds
    lds si, es:[bx + 0x0D]
    mov cx, es:[bx + 0x11]
    jcxz .dci_in_ok
.dci_in_lp:
    mov ah, 0x00
    int 0x16
    mov [si], al
    inc si
    loop .dci_in_lp
.dci_in_ok:
    pop ds
    mov word ptr es:[bx + 3], DEV_STAT_DONE
    jmp .dci_done
.dci_ostat:
    mov word ptr es:[bx + 3], DEV_STAT_DONE
    jmp .dci_done
.dci_out:
    push ds
    lds si, es:[bx + 0x0D]
    mov cx, es:[bx + 0x11]
    jcxz .dci_out_ok
    /* Text mode → fast path; one CRTC sync after the batch. */
    push es
    mov ax, 0x0040
    mov es, ax
    cmp byte ptr es:[0x49], 4
    pop es
    jae .dci_out_bios
    mov byte ptr cs:[con_defer_crtc], 1
.dci_out_fast:
    lodsb
    call con_teletype
    loop .dci_out_fast
    mov byte ptr cs:[con_defer_crtc], 0
    call con_sync_crtc
    jmp .dci_out_ok
.dci_out_bios:
    lodsb
    call con_teletype
    loop .dci_out_bios
.dci_out_ok:
    pop ds
    mov word ptr es:[bx + 3], DEV_STAT_DONE
.dci_done:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    retf

/* Builtin NUL — discard OUTPUT. */
dev_nul_strategy:
    mov word ptr cs:[dev_nul_rh_off], bx
    mov word ptr cs:[dev_nul_rh_seg], es
    retf

dev_nul_interrupt:
    push ax
    push bx
    push es
    mov es, word ptr cs:[dev_nul_rh_seg]
    mov bx, word ptr cs:[dev_nul_rh_off]
    mov al, es:[bx + 2]
    cmp al, DEV_CMD_INIT
    jne .dni_stat
    lea ax, [dev_nul_hdr_end]
    mov es:[bx + 0x0E], ax
    mov es:[bx + 0x10], cs
.dni_stat:
    mov word ptr es:[bx + 3], DEV_STAT_DONE
    pop es
    pop bx
    pop ax
    retf

/*
 * Teletype AL → screen + COM1. Preserves AL.
 * Text mode: direct B800 + BDA cursor; CRTC unless con_defer_crtc.
 * Graphics: INT 10h AH=0E.
 */
con_teletype:
    push ax
    push bx
    push cx
    push dx
    push di
    push es
    push ds
    mov ah, al                   /* save char in AH for COM1 */
    mov bx, 0x0040
    mov ds, bx
    cmp byte ptr [0x49], 4       /* CRT mode */
    jae .ct_bios
    /* Text fast path — DS=BDA */
    cmp al, 0x0D
    je .ct_cr
    cmp al, 0x0A
    je .ct_lf
    cmp al, 0x08
    je .ct_bs
    cmp al, 0x07
    je .ct_bel
    /* Printable: write atrribute 07 at cursor */
    mov bl, [0x62]               /* page */
    xor bh, bh
    shl bx, 1
    mov dx, [0x50 + bx]          /* row/col */
    push ax                      /* AL=char */
    push dx                      /* MUL clobbers DX */
    mov al, [0x62]
    xor ah, ah
    mov cx, [0x4C]               /* CRT_LEN */
    mul cx
    mov di, ax
    pop dx
    mov al, dh
    xor ah, ah
    mov cl, [0x4A]               /* cols */
    mul cl
    mov bl, dl
    xor bh, bh
    add ax, bx
    shl ax, 1
    add di, ax
    mov ax, 0xB800
    mov es, ax
    pop ax
    mov ah, 0x07
    mov es:[di], ax
    /* advance cursor */
    mov bl, [0x62]
    xor bh, bh
    shl bx, 1
    inc byte ptr [0x50 + bx]
    mov al, [0x4A]
    cmp byte ptr [0x50 + bx], al
    jb .ct_crtc
    mov byte ptr [0x50 + bx], 0
    call con_tty_lf
    jmp .ct_crtc
.ct_cr:
    mov bl, [0x62]
    xor bh, bh
    shl bx, 1
    mov byte ptr [0x50 + bx], 0
    jmp .ct_crtc
.ct_lf:
    call con_tty_lf
    jmp .ct_crtc
.ct_bs:
    mov bl, [0x62]
    xor bh, bh
    shl bx, 1
    cmp byte ptr [0x50 + bx], 0
    je .ct_crtc
    dec byte ptr [0x50 + bx]
    jmp .ct_crtc
.ct_bel:
    push ax
    mov al, 0x07
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10                     /* BIOS BEL (no cursor move) */
    pop ax
    mov ah, al                   /* COM1 char */
    jmp .ct_com1                 /* BEL: skip CRTC */
.ct_crtc:
    cmp byte ptr cs:[con_defer_crtc], 0
    jne .ct_com1
    call con_sync_crtc
.ct_com1:
    pop ds
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    /* fall through to com1_out with AL preserved */
    jmp com1_out

.ct_bios:
    pop ds
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    push ax
    push bx
    push dx
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    pop dx
    pop bx
    pop ax
    /* fall through */
com1_out:
    push ax
    push dx
    mov ah, al
.wait_tx:
    mov dx, 0x3FD
    in al, dx
    test al, 0x20
    jz .wait_tx
    mov al, ah
    mov dx, 0x3F8
    out dx, al
    pop dx
    pop ax
    ret

/* DS=BDA. Advance row; scroll at 25 via INT 10h AH=06. Clobbers AX,BX,CX,DX. */
con_tty_lf:
    push ax
    push bx
    push cx
    push dx
    mov bl, [0x62]
    xor bh, bh
    shl bx, 1
    inc byte ptr [0x50 + bx + 1]
    cmp byte ptr [0x50 + bx + 1], 25
    jb .ctl_done
    mov byte ptr [0x50 + bx + 1], 24
    mov ax, 0x0601
    mov bh, 0x07
    xor cx, cx
    mov dx, 0x184F
    int 0x10
.ctl_done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* Program CRTC from BDA cursor (INT 10h AH=02). Preserves AX. */
con_sync_crtc:
    push ax
    push bx
    push dx
    push ds
    mov ax, 0x0040
    mov ds, ax
    mov bl, [0x62]
    xor bh, bh
    shl bx, 1
    mov dx, [0x50 + bx]
    mov ah, 0x02
    mov bh, [0x62]
    int 0x10
    pop ds
    pop dx
    pop bx
    pop ax
    ret

/*
 * Invoke strategy then interrupt for header at con_dev_* / explicit
 * CS:dev_call_off / CS:dev_call_seg. Request at CS:dev_req.
 */
dev_call_driver:
    push ax
    push bx
    push es
    push ds
    push cs
    pop es
    lea bx, [dev_req]
    /* strategy */
    mov ax, word ptr cs:[dev_call_seg]
    mov ds, ax
    mov si, word ptr cs:[dev_call_off]
    mov ax, [si + 6]
    mov word ptr cs:[dev_far_off], ax
    mov word ptr cs:[dev_far_seg], ds
    push cs
    pop ds
    call dev_far_invoke
    /* interrupt — ES:BX must still be request; reload */
    push cs
    pop es
    lea bx, [dev_req]
    mov ax, word ptr cs:[dev_call_seg]
    mov ds, ax
    mov si, word ptr cs:[dev_call_off]
    mov ax, [si + 8]
    mov word ptr cs:[dev_far_off], ax
    mov word ptr cs:[dev_far_seg], ds
    push cs
    pop ds
    call dev_far_invoke
    pop ds
    pop es
    pop bx
    pop ax
    ret

/* Far call through CS:dev_far_off / CS:dev_far_seg (contiguous DWORD). */
dev_far_invoke:
    push cs
    lea ax, [.dfi_ret]
    push ax
    push word ptr cs:[dev_far_seg]
    push word ptr cs:[dev_far_off]
    retf
.dfi_ret:
    ret

/* OUTPUT CX bytes at DS:DX through current CON driver. */
con_dev_write:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds
    mov word ptr cs:[dev_req], DEV_REQ_SIZE
    mov byte ptr cs:[dev_req + 1], 0
    mov byte ptr cs:[dev_req + 2], DEV_CMD_OUTPUT
    mov word ptr cs:[dev_req + 3], 0
    mov word ptr cs:[dev_req + 0x0D], dx
    mov word ptr cs:[dev_req + 0x0F], ds
    mov word ptr cs:[dev_req + 0x11], cx
    mov ax, word ptr cs:[con_dev_off]
    mov word ptr cs:[dev_call_off], ax
    mov ax, word ptr cs:[con_dev_seg]
    mov word ptr cs:[dev_call_seg], ax
    call dev_call_driver
    pop ds
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* INPUT CX bytes at DS:DX through current CON driver. */
con_dev_read:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds
    mov word ptr cs:[dev_req], DEV_REQ_SIZE
    mov byte ptr cs:[dev_req + 1], 0
    mov byte ptr cs:[dev_req + 2], DEV_CMD_INPUT
    mov word ptr cs:[dev_req + 3], 0
    mov word ptr cs:[dev_req + 0x0D], dx
    mov word ptr cs:[dev_req + 0x0F], ds
    mov word ptr cs:[dev_req + 0x11], cx
    mov ax, word ptr cs:[con_dev_off]
    mov word ptr cs:[dev_call_off], ax
    mov ax, word ptr cs:[con_dev_seg]
    mov word ptr cs:[dev_call_seg], ax
    call dev_call_driver
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
 * DEVICE=.SYS loader. DS:DX = ASCIZ path.
 * CF set on failure.
 */
load_device_sys:
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
    mov byte ptr cs:[device_load_error], 0

    mov ah, 0x3D
    xor al, al
    int 0x21
    jc .lds_fail
    mov word ptr cs:[com_handle], bx

    call exec_file_size
    jc .lds_fail_close
    test dx, dx
    jnz .lds_fail_close
    test ax, ax
    jz .lds_fail_close
    cmp ax, SYS_MAX
    jbe .lds_size_ok
    mov byte ptr cs:[device_load_error], 1
    jmp .lds_fail_close
.lds_size_ok:
    mov word ptr cs:[com_size], ax
    mov word ptr cs:[img_bytes_lo], ax
    mov word ptr cs:[img_bytes_hi], 0

    /* paragraphs = ceil(size/16) */
    add ax, 15
    mov cl, 4
    shr ax, cl
    mov bx, ax
    mov ah, 0x48
    int 0x21
    jc .lds_fail_close
    mov word ptr [psp_run], ax

    mov bx, word ptr cs:[com_handle]
    mov ax, 0x4200
    xor cx, cx
    xor dx, dx
    int 0x21
    jc .lds_stream_fail
    mov es, word ptr [psp_run]
    xor di, di
    call stream_file_to_esdi
    jc .lds_stream_fail

    mov ah, 0x3E
    mov bx, word ptr [com_handle]
    int 0x21

    /* link at chain head: driver.next = old chain; chain = driver */
    mov es, word ptr [psp_run]
    mov ax, word ptr [dev_chain_off]
    mov es:[0], ax
    mov ax, word ptr [dev_chain_seg]
    mov es:[2], ax
    xor ax, ax
    mov word ptr [dev_chain_off], ax
    mov ax, word ptr [psp_run]
    mov word ptr [dev_chain_seg], ax

    /* INIT */
    mov byte ptr [dev_req], DEV_REQ_SIZE
    mov byte ptr [dev_req + 1], 0
    mov byte ptr [dev_req + 2], DEV_CMD_INIT
    mov word ptr [dev_req + 3], 0
    /* default break = full image */
    mov ax, word ptr [com_size]
    mov word ptr [dev_req + 0x0E], ax
    mov ax, word ptr [psp_run]
    mov word ptr [dev_req + 0x10], ax

    mov word ptr [dev_call_off], 0
    mov ax, word ptr [psp_run]
    mov word ptr [dev_call_seg], ax
    call dev_call_driver

    test word ptr [dev_req + 3], 0x8000
    jnz .lds_fail_free

    /* Block drivers need a different ABI; retain character drivers only. */
    mov es, word ptr [psp_run]
    test word ptr es:[4], DEV_ATTR_CHAR
    jnz .lds_is_char
    mov byte ptr cs:[device_load_error], 2
    jmp .lds_fail_free
.lds_is_char:

    /* shrink to break address */
    mov ax, word ptr [dev_req + 0x0E]
    add ax, 15
    mov cl, 4
    shr ax, cl
    test ax, ax
    jnz .lds_shr
    mov ax, 1
.lds_shr:
    mov bx, ax
    mov es, word ptr [psp_run]
    mov ah, 0x4A
    int 0x21

    /* If name CON or stdout bit, become current CON */
    mov es, word ptr [psp_run]
    mov ax, es:[4]
    test ax, DEV_ATTR_STDOUT
    jnz .lds_set_con
    cmp word ptr es:[0x0A], 0x4F43       /* "CO" */
    jne .lds_ok
    cmp word ptr es:[0x0C], 0x204E       /* "N " */
    jne .lds_ok
    cmp word ptr es:[0x0E], 0x2020
    jne .lds_ok
.lds_set_con:
    xor ax, ax
    mov word ptr [con_dev_off], ax
    mov ax, word ptr [psp_run]
    mov word ptr [con_dev_seg], ax
    mov word ptr cs:[dos_dpb + 0x13], 0
    mov word ptr cs:[dos_dpb + 0x15], ax
.lds_ok:
    clc
    jmp .lds_done

.lds_stream_fail:
    mov ah, 0x3E
    mov bx, word ptr cs:[com_handle]
    int 0x21
    mov es, word ptr cs:[psp_run]
    mov ah, 0x49
    int 0x21
    jmp .lds_fail
.lds_fail_close:
    mov ah, 0x3E
    mov bx, word ptr cs:[com_handle]
    int 0x21
    jmp .lds_fail
.lds_fail_free:
    /* unlink before free */
    mov es, word ptr cs:[psp_run]
    mov ax, es:[0]
    mov word ptr cs:[dev_chain_off], ax
    mov ax, es:[2]
    mov word ptr cs:[dev_chain_seg], ax
    mov ah, 0x49
    int 0x21
.lds_fail:
    stc
.lds_done:
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
 * DS:SI = ASCIZ path. CF clear if ends with .SYS (case-insensitive).
 * Preserves SI.
 */
cfg_path_is_sys:
    push ax
    push bx
    push si
    mov bx, si
.cpis_end:
    cmp byte ptr [si], 0
    je .cpis_got
    inc si
    jmp .cpis_end
.cpis_got:
    /* need at least ".SYS" */
    mov ax, si
    sub ax, bx
    cmp ax, 4
    jb .cpis_no
    mov al, [si - 4]
    cmp al, '.'
    jne .cpis_no
    mov al, [si - 3]
    or al, 0x20
    cmp al, 's'
    jne .cpis_no
    mov al, [si - 2]
    or al, 0x20
    cmp al, 'y'
    jne .cpis_no
    mov al, [si - 1]
    or al, 0x20
    cmp al, 's'
    jne .cpis_no
    clc
    jmp .cpis_done
.cpis_no:
    stc
.cpis_done:
    pop si
    pop bx
    pop ax
    ret
