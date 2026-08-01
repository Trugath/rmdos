.code16
.intel_syntax noprefix

.include "firmware/bios/inc/equates.inc"

/*
 * rmDOS XT floppy FDC helpers + INT 0Eh (IRQ6).
 * Diskette geometry/gaps come from the INT 1Eh DPT (disk.s).
 */

.section .text
.global isr_0e
.global fdc_reset, fdc_motor_on, fdc_seek
.global fdc_setup_dma, fdc_do_rw, fdc_do_format
.global fdc_read_dir, fdc_store_status
.global fdc_dpt_ptr

/* DS:BX â†’ current INT 1Eh diskette parameter table. Clobbers AX, ES. */
fdc_dpt_ptr:
    xor ax, ax
    mov es, ax
    mov bx, es:[0x1E * 4]
    mov ax, es:[0x1E * 4 + 2]
    mov ds, ax
    ret

isr_0e:
    push ax
    push ds
    xor ax, ax
    mov ds, ax
    mov byte ptr [FDC_IRQ_FLAG], 1
    mov al, 0x20
    out PORT_PIC_CMD, al
    pop ds
    pop ax
    iret

fdc_clear_irq:
    push ax
    push ds
    xor ax, ax
    mov ds, ax
    mov byte ptr [FDC_IRQ_FLAG], 0
    pop ds
    pop ax
    ret

/* Wait for FDC IRQ and/or result-phase (MSR DIO). Timeout â†’ AH=80h CF. */
fdc_wait_irq:
    push bx
    push cx
    push dx
    push ds
    push es
    mov ax, BDA_SEG
    mov ds, ax
    xor ax, ax
    mov es, ax
    mov bx, [BDA_TIMER_LO]
    mov cx, 0x8000               /* software bound if timer/IRQ stall */
.fwi_loop:
    cmp byte ptr es:[FDC_IRQ_FLAG], 0
    jne .fwi_ok
    mov dx, PORT_FDC_MSR
    in al, dx
    test al, 0x40                /* DIO = result phase */
    jnz .fwi_ok
    mov ax, [BDA_TIMER_LO]
    sub ax, bx
    cmp ax, 37
    jae .fwi_to
    dec cx
    jz .fwi_to
    sti
    hlt
    jmp .fwi_loop
.fwi_ok:
    xor ah, ah
    clc
    jmp .fwi_done
.fwi_to:
    mov ah, 0x80
    stc
.fwi_done:
    pop es
    pop ds
    pop dx
    pop cx
    pop bx
    ret

fdc_wait_rqm:
    push ax
    push cx
    push dx
    mov cx, 0xFFFF
.fwr_loop:
    mov dx, PORT_FDC_MSR
    in al, dx
    test al, 0x80
    jz .fwr_next
    test bh, bh
    jnz .fwr_rd
    test al, 0x40
    jnz .fwr_next
    jmp .fwr_ok
.fwr_rd:
    test al, 0x40
    jz .fwr_next
.fwr_ok:
    clc
    jmp .fwr_done
.fwr_next:
    loop .fwr_loop
    stc
.fwr_done:
    pop dx
    pop cx
    pop ax
    ret

fdc_send_byte:
    push bx
    xor bh, bh
    call fdc_wait_rqm
    jc .fsb_fail
    push dx
    mov dx, PORT_FDC_DATA
    out dx, al
    pop dx
    clc
.fsb_fail:
    pop bx
    ret

fdc_recv_byte:
    push bx
    mov bh, 1
    call fdc_wait_rqm
    jc .frb_fail
    push dx
    mov dx, PORT_FDC_DATA
    in al, dx
    pop dx
    clc
.frb_fail:
    pop bx
    ret

fdc_sense_int:
    mov al, 0x08
    call fdc_send_byte
    jc .fsi_err
    call fdc_recv_byte
    jc .fsi_err
    mov ah, al
    call fdc_recv_byte
    jc .fsi_err
    xchg al, ah
    clc
    ret
.fsi_err:
    stc
    ret

fdc_read_dir:
    push dx
    mov dx, PORT_FDC_DIR
    in al, dx
    pop dx
    ret

fdc_store_status:
    push ds
    push bx
    mov bx, BDA_SEG
    mov ds, bx
    mov [BDA_FLOPPY_STATUS], ah
    pop bx
    pop ds
    test ah, ah
    jz .fss_ok
    stc
    ret
.fss_ok:
    clc
    ret

fdc_reset:
    push ax
    push cx
    push dx
    push ds
    call fdc_clear_irq
    mov ax, BDA_SEG
    mov ds, ax
    mov byte ptr [BDA_FLOPPY_STATUS], 0
    mov dx, PORT_FDC_DOR
    xor al, al
    out dx, al
    mov cx, 0x80
.frst_dly:
    loop .frst_dly
    mov al, 0x0C
    out dx, al
    call fdc_wait_irq
    /* Drain reset Sense Interrupt Status (4 drives), ignore timeouts. */
    mov cx, 4
.frst_sis:
    call fdc_sense_int
    loop .frst_sis
    call fdc_clear_irq
    mov al, 0x03
    call fdc_send_byte
    /* Specify from INT 1Eh DPT (DS currently BDA; fdc_dpt_ptr replaces it). */
    call fdc_dpt_ptr
    mov al, [bx]
    call fdc_send_byte
    mov al, [bx + 1]
    call fdc_send_byte
    pop ds
    pop dx
    pop cx
    pop ax
    clc
    ret

fdc_motor_on:
    push ax
    push bx
    push cx
    push dx
    push ds
    push es
    mov ax, BDA_SEG
    mov ds, ax
    mov bl, dl
    and bl, 3
    mov cl, bl
    mov al, 0x10
    shl al, cl
    or al, 0x0C
    or al, bl
    mov dx, PORT_FDC_DOR
    out dx, al
    mov ah, 0x10
    mov cl, bl
    shl ah, cl
    or [BDA_FLOPPY_MOTOR], ah
    push ds
    push bx
    call fdc_dpt_ptr
    mov al, [bx + 2]
    mov cl, [bx + 10]
    pop bx
    pop ds
    mov [BDA_FLOPPY_TIMEOUT], al
    test cl, cl
    jz .fmo_done
.fmo_has:
    mov bx, [BDA_TIMER_LO]
.fmo_wait:
    mov ax, [BDA_TIMER_LO]
    sub ax, bx
    cmp al, cl
    jae .fmo_done
    sti
    hlt
    jmp .fmo_wait
.fmo_done:
    pop es
    pop ds
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret

fdc_seek:
    push bx
    push cx
    push dx
    push si
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov bl, dl
    and bx, 3
    mov si, cx                   /* SI keeps target CH (delay clobbers CX) */
    /* Always recalibrate when target is cylinder 0; otherwise seek if needed. */
    test ch, ch
    jz .fsk_recal
    cmp byte ptr [BDA_FLOPPY_CYL + bx], ch
    je .fsk_ok
    call fdc_clear_irq
    mov al, 0x0F
    call fdc_send_byte
    jc .fsk_fail
    mov al, bl
    call fdc_send_byte
    jc .fsk_fail
    mov ax, si
    mov al, ah                   /* cylinder from saved CH */
    call fdc_send_byte
    jc .fsk_fail
    mov cx, 0x40
.fsk_seek_delay:
    loop .fsk_seek_delay
    call fdc_sense_int
    jc .fsk_fail
    mov ax, si
    mov byte ptr [BDA_FLOPPY_CYL + bx], ah
    jmp .fsk_ok
.fsk_recal:
    call fdc_clear_irq
    mov al, 0x07
    call fdc_send_byte
    jc .fsk_fail
    mov al, bl
    call fdc_send_byte
    jc .fsk_fail
    mov cx, 0x40
.fsk_recal_delay:
    loop .fsk_recal_delay
    call fdc_sense_int
    jc .fsk_fail
    mov byte ptr [BDA_FLOPPY_CYL + bx], 0
.fsk_ok:
    xor ah, ah
    clc
    jmp .fsk_out
.fsk_fail:
    mov ah, 0x40
    stc
.fsk_out:
    pop ds
    pop si
    pop dx
    pop cx
    pop bx
    ret

/*
 * Setup DMA ch2.
 * IN: ES:BX = buffer, CX = byte count, AL = 0 (floppyâ†’mem) or 1 (memâ†’floppy)
 * OUT: CF set on 64K boundary cross.
 */
fdc_setup_dma:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    /* stack: DI SI DX CX BX AX */
    /* phys = (ES<<4)+BX */
    mov ax, es
    mov dx, ax
    mov cl, 4
    shl ax, cl
    mov si, ax
    mov ax, dx
    mov cl, 12
    shr ax, cl
    add si, bx
    adc al, 0
    mov di, ax                   /* page */
    mov bx, sp
    mov cx, ss:[bx + 6]          /* CX: di0 si2 dx4 cx6 bx8 ax10 */
    mov ax, si
    add ax, cx
    jc .fsd_bad
    dec cx
    /* Mask ch2 while programming */
    mov al, 0x06
    mov dx, PORT_DMA_MASK
    out dx, al
    mov dx, PORT_DMA_FF
    out dx, al
    mov dx, PORT_DMA_CH2_ADDR
    mov ax, si
    out dx, al
    mov al, ah
    out dx, al
    mov dx, PORT_DMA_FF
    out dx, al
    mov dx, PORT_DMA_CH2_CNT
    mov ax, cx
    out dx, al
    mov al, ah
    out dx, al
    mov dx, PORT_DMA_PAGE_CH2
    mov ax, di
    out dx, al
    mov bx, sp
    mov ax, ss:[bx + 10]
    cmp al, 0
    je .fsd_rd
    mov al, 0x4A
    jmp .fsd_mode
.fsd_rd:
    mov al, 0x46
.fsd_mode:
    mov dx, PORT_DMA_MODE
    out dx, al
    mov al, 0x02                 /* unmask ch2 */
    mov dx, PORT_DMA_MASK
    out dx, al
    clc
    jmp .fsd_done
.fsd_bad:
    stc
.fsd_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * Read/Write sectors via FDC.
 * IN: AH=0 read / 1 write / 2 verify(read to FDC_VERIFY_BUF),
 *     AL=sector count, DL=drive, DH=head, CH=cyl, CL=sector,
 *     ES:BX=buffer (ignored for verify).
 * OUT: AH=BIOS status, CF, AL=sectors requested on success.
 */
fdc_do_rw:
    push bp
    mov bp, sp
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    mov si, ax                   /* SI: AH=mode AL=count */
    mov di, cx                   /* DI: CH=cyl CL=sector */
    call fdc_motor_on
    call fdc_seek
    jc .fdr_fail_seek

    /* CX = AL * 512 */
    mov ax, si
    xor ah, ah
    mov cl, 9
    shl ax, cl
    mov cx, ax
    mov ax, si
    cmp ah, 2
    jne .fdr_usebuf
    xor ax, ax
    mov es, ax
    mov bx, FDC_VERIFY_BUF
    mov cx, 512
.fdr_usebuf:
    mov ax, si
    mov al, 0
    cmp ah, 1
    jne .fdr_dma
    mov al, 1
.fdr_dma:
    call fdc_setup_dma
    jc .fdr_bound

    call fdc_clear_irq
    mov ax, si
    cmp ah, 1
    je .fdr_wcmd
    mov al, 0x46
    jmp .fdr_scmd
.fdr_wcmd:
    mov al, 0x45
.fdr_scmd:
    call fdc_send_byte
    jc .fdr_to
    /* (H<<2)|drive from saved DX at [bp-6] */
    mov dx, [bp - 6]
    mov al, dh
    and al, 1
    shl al, 1
    shl al, 1
    mov ah, dl
    and ah, 3
    or al, ah
    call fdc_send_byte
    jc .fdr_to
    mov ax, di
    mov al, ah                   /* cylinder */
    call fdc_send_byte
    jc .fdr_to
    mov dx, [bp - 6]
    mov al, dh
    and al, 1
    call fdc_send_byte
    jc .fdr_to
    mov ax, di
    and al, 0x3F                 /* sector */
    call fdc_send_byte
    jc .fdr_to
    push ds
    call fdc_dpt_ptr
    mov al, [bx + 3]
    call fdc_send_byte
    jc .fdr_to_dpt
    mov al, [bx + 4]
    call fdc_send_byte
    jc .fdr_to_dpt
    mov al, [bx + 5]
    call fdc_send_byte
    jc .fdr_to_dpt
    mov al, [bx + 6]
    call fdc_send_byte
    jc .fdr_to_dpt
    pop ds
    call fdc_wait_irq
    jc .fdr_to

    mov ax, BDA_SEG
    mov es, ax
    mov bx, BDA_FLOPPY_NEC
    mov cx, 7
.fdr_res:
    call fdc_recv_byte
    jc .fdr_to
    mov es:[bx], al
    inc bx
    loop .fdr_res
    mov ah, es:[BDA_FLOPPY_NEC]
    and ah, 0xC0
    jnz .fdr_err
    mov ax, si
    xor ah, ah
    clc
    jmp .fdr_done
.fdr_err:
    mov al, es:[BDA_FLOPPY_NEC + 1]
    test al, 0x02
    jz .fdr_e1
    mov ah, 0x03
    jmp .fdr_fail
.fdr_e1:
    test al, 0x10
    jz .fdr_e2
    mov ah, 0x10
    jmp .fdr_fail
.fdr_e2:
    test al, 0x04
    jz .fdr_e3
    mov ah, 0x04
    jmp .fdr_fail
.fdr_e3:
    mov ah, 0x20
    jmp .fdr_fail
.fdr_bound:
    mov ah, 0x09
    jmp .fdr_fail
.fdr_to_dpt:
    pop ds
.fdr_to:
    mov ah, 0x80
    jmp .fdr_fail
.fdr_fail_seek:
    /* AH already set by fdc_seek */
.fdr_fail:
    stc
.fdr_done:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop bp
    ret

/*
 * Format one track.
 * IN: DL=drive, DH=head, CH=cyl, ES:BX = SC×(C,H,R,N)
 */
fdc_do_format:
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    mov di, dx                   /* DI: DH=head DL=drive */
    call fdc_motor_on
    call fdc_seek
    jc .fdf_fail
    push ds
    push bx                      /* ID table offset */
    call fdc_dpt_ptr
    mov al, [bx + 3]             /* N */
    mov ah, [bx + 4]             /* SC */
    mov cl, [bx + 7]             /* gap */
    mov ch, [bx + 8]             /* fill */
    pop bx                       /* restore ID table */
    pop ds
    push ax                      /* AL=N AH=SC */
    push cx                      /* CL=gap CH=fill */
    mov al, ah                   /* SC from saved AH — still in AX */
    xor ah, ah
    shl ax, 1
    shl ax, 1
    mov cx, ax
    mov al, 1
    call fdc_setup_dma
    jc .fdf_bound_stk
    call fdc_clear_irq
    mov al, 0x4D
    call fdc_send_byte
    jc .fdf_to_stk
    mov ax, di
    mov cl, al                   /* drive */
    mov al, ah                   /* head */
    and al, 1
    shl al, 1
    shl al, 1
    and cl, 3
    or al, cl
    call fdc_send_byte
    jc .fdf_to_stk
    pop cx                       /* CL=gap CH=fill — wait, stack order: top is gap/fill */
    pop ax                       /* AL=N AH=SC */
    push cx                      /* keep gap/fill for later */
    call fdc_send_byte           /* N in AL */
    jc .fdf_to_one
    mov al, ah                   /* SC */
    call fdc_send_byte
    jc .fdf_to_one
    pop cx                       /* CL=gap CH=fill */
    mov al, cl
    call fdc_send_byte
    jc .fdf_to
    mov al, ch
    call fdc_send_byte
    jc .fdf_to
    call fdc_wait_irq
    jc .fdf_to
    mov cx, 7
.fdf_res:
    call fdc_recv_byte
    jc .fdf_to
    loop .fdf_res
    xor ah, ah
    clc
    jmp .fdf_done
.fdf_bound_stk:
    pop cx
    pop ax
.fdf_bound:
    mov ah, 0x09
    jmp .fdf_fail
.fdf_to_stk:
    pop cx
    pop ax
    jmp .fdf_to
.fdf_to_one:
    pop cx
.fdf_to:
    mov ah, 0x80
.fdf_fail:
    stc
.fdf_done:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ret
