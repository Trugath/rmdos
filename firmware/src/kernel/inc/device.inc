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
.dci_out_lp:
    lodsb
    call con_teletype
    loop .dci_out_lp
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

/* Teletype AL → CGA + COM1. Preserves AL. Used by builtin CON. */
con_teletype:
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

    mov ah, 0x3D
    xor al, al
    int 0x21
    jc .lds_fail
    mov word ptr cs:[com_handle], bx

    push cs
    pop ds
    mov ah, 0x3F
    mov bx, word ptr [com_handle]
    mov cx, SYS_MAX
    lea dx, [com_buf]
    int 0x21
    jc .lds_fail_close
    mov word ptr [com_size], ax
    test ax, ax
    jz .lds_fail_close

    mov ah, 0x3E
    mov bx, word ptr [com_handle]
    int 0x21

    /* paragraphs = ceil(size/16) */
    mov ax, word ptr [com_size]
    add ax, 15
    mov cl, 4
    shr ax, cl
    mov bx, ax
    mov ah, 0x48
    int 0x21
    jc .lds_fail
    mov word ptr [psp_run], ax

    /* copy image to alloc:0000 */
    mov es, ax
    xor di, di
    lea si, [com_buf]
    mov cx, word ptr [com_size]
    push ds
    push cs
    pop ds
    rep movsb
    pop ds

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
