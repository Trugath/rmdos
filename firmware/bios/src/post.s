.code16
.intel_syntax noprefix

.include "firmware/bios/inc/equates.inc"

/*
 * rmDOS clean-room XT system BIOS — POST flow and diagnostics.
 * Pinned stubs in bios_entries.s.
 */

.section .text
.global post_main

post_main:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7000             /* keep stack clear of IVT (0..0x3FF) and BDA */

    mov al, 0x99
    out PORT_PPI_CTL, al
    mov al, 0x00
    out PORT_PPI_B, al

    call init_bda
    call init_ivt
    call init_pic
    call init_pit
    call init_cga
    sti                         /* keyboard available during POST */

    push cs
    pop ds
    mov si, offset post_banner
    call post_puts

    /* Esc skips remaining diagnostics (INT 09h sets POST_SKIP while active). */
    xor ax, ax
    mov ds, ax
    mov byte ptr [POST_SKIP], 0
    mov byte ptr [POST_ERRORS], 0
    mov byte ptr [POST_ACTIVE], 1

    call post_check_esc
    jnc .post_do_checks
    push cs
    pop ds
    mov si, offset msg_post_skip
    call post_puts
    jmp .post_after_checks

.post_do_checks:
    call post_diagnostics

.post_after_checks:
    xor ax, ax
    mov ds, ax
    mov byte ptr [POST_ACTIVE], 0
    mov byte ptr [POST_SKIP], 0

    call size_memory            /* always refresh MEMKB (skips probe on warm) */
    call scan_option_roms

    xor ax, ax
    mov ds, ax
    cmp byte ptr [POST_ERRORS], 0
    je .post_boot

    push cs
    pop ds
    mov si, offset msg_error_f1
    call post_puts
    /* k8086 injects F1 when CGA shows RESUME = (see Machine.pollPostResumeF1). */
    call f1_entry

.post_boot:
    call post_clear_screen
    int 0x19
    int 0x18
.hang:
    hlt
    jmp .hang

/* Blank 80×25 text page and home the cursor (INT 10h). */
post_clear_screen:
    push ax
    push bx
    push cx
    push dx
    mov ax, 0x0600
    mov bh, 0x07
    xor cx, cx
    mov dx, 0x184F
    int 0x10
    xor dx, dx
    mov bh, 0
    mov ah, 0x02
    int 0x10
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* DS:SI ASCIZ via INT 10h teletype */
post_puts:
    push ax
    push bx
    push si
    mov ah, 0x0E
    mov bh, 0
.pp_loop:
    lodsb
    test al, al
    jz .pp_done
    int 0x10
    jmp .pp_loop
.pp_done:
    pop si
    pop bx
    pop ax
    ret

/* Print AX as unsigned decimal (no leading zeros except 0). */
post_put_dec:
    push ax
    push bx
    push cx
    push dx
    mov bx, 10
    xor cx, cx
.pd_div:
    xor dx, dx
    div bx
    push dx
    inc cx
    test ax, ax
    jnz .pd_div
.pd_out:
    pop dx
    mov ah, 0x0E
    mov al, dl
    add al, '0'
    mov bh, 0
    int 0x10
    loop .pd_out
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/*
 * Esc skip: INT 09h latches POST_SKIP while POST_ACTIVE.
 * Loops just test the flag (CF=1 if set) — no INT 16h needed.
 */
post_check_esc:
    push ax
    push ds
    xor ax, ax
    mov ds, ax
    cmp byte ptr [POST_SKIP], 0
    pop ds
    pop ax
    jne .pce_yes
    clc
    ret
.pce_yes:
    stc
    ret

/* Record IBM-like error code in AX (e.g. 201), print it, bump error count. */
post_report_error:
    push ds
    push si
    push ax
    xor ax, ax
    mov ds, ax
    pop ax
    mov [POST_ERRCODE], ax
    inc byte ptr [POST_ERRORS]
    push cs
    pop ds
    call post_put_dec
    mov si, offset msg_crlf
    call post_puts
    pop si
    pop ds
    ret

post_diagnostics:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es

    push cs
    pop ds
    mov si, offset msg_post_hint
    call post_puts

    call post_check_esc
    jc .pd_done

    call post_test_pic
    call post_check_esc
    jc .pd_done

    call post_test_pit
    call post_check_esc
    jc .pd_done

    call post_test_video
    call post_check_esc
    jc .pd_done

    call post_test_keyboard
    call post_check_esc
    jc .pd_done

    call post_test_memory
    call post_check_esc
    jc .pd_done

    call post_test_equipment

    xor ax, ax
    mov ds, ax
    cmp byte ptr [POST_ERRORS], 0
    jne .pd_done
    push cs
    pop ds
    mov si, offset msg_post_ok
    call post_puts
    call post_beep_ok

.pd_done:
    xor ax, ax
    mov ds, ax
    cmp byte ptr [POST_SKIP], 0
    je .pd_ret
    push cs
    pop ds
    mov si, offset msg_post_skip
    call post_puts
.pd_ret:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* 101 — interrupt controller IMR r/w (CLI so IRQ0 cannot preempt readback). */
post_test_pic:
    pushf
    cli
    mov al, 0xFF
    out PORT_PIC_DATA, al
    jmp .+2
    jmp .+2
    in al, PORT_PIC_DATA
    cmp al, 0xFF
    jne .ptpic_fail
    mov al, 0xFC                 /* unmask IRQ0+IRQ1 */
    out PORT_PIC_DATA, al
    jmp .+2
    jmp .+2
    in al, PORT_PIC_DATA
    cmp al, 0xFC
    jne .ptpic_fail
    popf
    ret
.ptpic_fail:
    mov al, 0xFC
    out PORT_PIC_DATA, al
    popf
    mov ax, 101
    call post_report_error
    ret

/*
 * Timer channel 0 advances BDA ticks.
 * Bounded spin (no HLT) so a silent PIT cannot stall headless POST forever.
 */
post_test_pit:
    push ds
    push bx
    push cx
    push si
    mov ax, BDA_SEG
    mov ds, ax
    mov si, [BDA_TIMER_LO]       /* baseline — SI survives INT 16h */
    mov cx, 0x4000
.ptpit_wait:
    test cl, 0x3F
    jnz .ptpit_poll
    call post_check_esc
    jc .ptpit_done
.ptpit_poll:
    mov ax, [BDA_TIMER_LO]
    cmp ax, si
    jne .ptpit_done
    loop .ptpit_wait
    mov ax, 106
    call post_report_error
.ptpit_done:
    pop si
    pop cx
    pop bx
    pop ds
    ret

/* Video regen read/write at B800:0100 (avoid clobbering POST banner). */
post_test_video:
    push es
    mov ax, CGA_SEG
    mov es, ax
    mov word ptr es:[0x100], 0xAA55
    cmp word ptr es:[0x100], 0xAA55
    jne .ptv_fail
    mov word ptr es:[0x100], 0x55AA
    cmp word ptr es:[0x100], 0x55AA
    jne .ptv_fail
    mov word ptr es:[0x100], 0x0720
    pop es
    ret
.ptv_fail:
    mov word ptr es:[0x100], 0x0720
    pop es
    mov ax, 401
    call post_report_error
    ret

/*
 * Keyboard presence: XT has no BAT; do not fail POST on noisy PPI data.
 * (Stuck-key 301 is reserved for a future manufacturing path.)
 */
post_test_keyboard:
    ret

/*
 * Conventional memory probe with IBM-style "xxxxx KB" progress.
 * Failure → 201. Esc aborts remaining blocks (not an error).
 *
 * Below 32KB: sparse word checks (stack lives at 0000:7000).
 * 32KB..640KB: fill/verify each 1KB with AA55 then 55AA (real work;
 * feels slow under k8086 realtime pacing, still cheap headless/CI).
 */
post_test_memory:
    push ds
    push es
    push bx
    push cx
    push dx
    push si
    push di

    mov ax, BDA_SEG
    mov ds, ax
    cmp word ptr [BDA_WARM_FLAG], WARM_BOOT_MAGIC
    je .ptm_warm

    /* Own line for the KB counter (in-place CR updates). */
    push cs
    pop ds
    mov si, offset msg_crlf
    call post_puts

    mov bx, 0x0400              /* start at 16KB — protect IVT/BDA */
.ptm_block:
    cmp bx, 0xA000
    jae .ptm_done

    /* Esc aborts remaining RAM test (checked every KB). */
    call post_check_esc
    jc .ptm_abort

    cmp bx, 0x0800              /* 32KB — above POST stack */
    jae .ptm_full
    call post_mem_sparse
    jc .ptm_fail
    call post_check_esc
    jc .ptm_abort
    jmp .ptm_after
.ptm_full:
    call post_mem_fill_block
    jc .ptm_fail
    call post_check_esc
    jc .ptm_abort

.ptm_after:
    add bx, 0x40                /* +1KB completed */
    /* Count up every KB so the progress is visible. */
    mov ax, bx
    mov cl, 6
    shr ax, cl
    call post_put_kb_line
    jmp .ptm_block

.ptm_fail:
    mov ax, bx
    mov cl, 6
    shr ax, cl
    call post_put_kb_line
    mov ax, 201
    call post_report_error
    mov ax, bx
    mov cl, 6
    shr ax, cl
    mov dx, BDA_SEG
    mov ds, dx
    mov [BDA_MEMKB], ax
    jmp .ptm_out

.ptm_done:
    mov ax, bx
    mov cl, 6
    shr ax, cl
    call post_put_kb_line
    push cs
    pop ds
    mov si, offset msg_kb_ok
    call post_puts
    mov dx, BDA_SEG
    mov ds, dx
    mov [BDA_MEMKB], ax
    jmp .ptm_out

.ptm_abort:
    mov ax, bx
    mov cl, 6
    shr ax, cl
    test ax, ax
    jnz .ptm_abort_store
    mov ax, 64
.ptm_abort_store:
    call post_put_kb_line
    mov dx, BDA_SEG
    mov ds, dx
    mov [BDA_MEMKB], ax
    jmp .ptm_out

.ptm_warm:
    push cs
    pop ds
    mov si, offset msg_crlf
    call post_puts
    mov ax, BDA_SEG
    mov ds, ax
    mov ax, [BDA_MEMKB]
    call post_put_kb_line
    push cs
    pop ds
    mov si, offset msg_kb_ok
    call post_puts

.ptm_out:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop es
    pop ds
    ret

/* Sparse AA55/55AA/0101 at a few offsets. BX=paragraph. CF=1 on fail. */
post_mem_sparse:
    push ax
    push di
    mov es, bx
    mov di, 0x0002
.ptms_off:
    mov word ptr es:[di], 0xAA55
    cmp word ptr es:[di], 0xAA55
    jne .ptms_bad
    mov word ptr es:[di], 0x55AA
    cmp word ptr es:[di], 0x55AA
    jne .ptms_bad
    mov word ptr es:[di], 0x0101
    cmp word ptr es:[di], 0x0101
    jne .ptms_bad
    mov word ptr es:[di], 0
    add di, 0x100
    cmp di, 0x400
    jb .ptms_off
    pop di
    pop ax
    clc
    ret
.ptms_bad:
    pop di
    pop ax
    stc
    ret

/* Fill/verify 1KB at paragraph BX with AA55 then 55AA; leave zeros. CF=1 fail.
 * Esc (POST_SKIP via INT 09h) aborts mid-block with CF=0; caller rechecks flag.
 */
post_mem_fill_block:
    push ax
    push cx
    push di
    cld
    mov es, bx
    mov ax, 0xAA55
    call .ptmf_pass
    jc .ptmf_out
    call post_check_esc
    jc .ptmf_esc
    mov ax, 0x55AA
    call .ptmf_pass
    jc .ptmf_out
    call post_check_esc
    jc .ptmf_esc
    xor ax, ax
    call .ptmf_write_only
    clc
.ptmf_out:
    pop di
    pop cx
    pop ax
    ret
.ptmf_esc:
    xor ax, ax
    call .ptmf_write_only
    clc
    jmp .ptmf_out

/* AX=pattern: write+verify 512 words, polling POST_SKIP every 64 words. */
.ptmf_pass:
    push ax
    call .ptmf_write_only
    pop ax
    jc .ptmf_pass_esc
    xor di, di
    mov cx, 512
.ptmf_cmp_chunk:
    push cx
    mov cx, 64
.ptmf_cmp:
    scasw
    loope .ptmf_cmp
    pop cx
    jne .ptmf_bad
    sub cx, 64
    push ds
    push bx
    xor bx, bx
    mov ds, bx
    cmp byte ptr [POST_SKIP], 0
    pop bx
    pop ds
    jne .ptmf_pass_esc
    test cx, cx
    jnz .ptmf_cmp_chunk
    clc
    ret
.ptmf_bad:
    stc
    ret
.ptmf_pass_esc:
    clc                          /* POST_SKIP set; fill_block / caller abort */
    ret

/* AX=pattern: store 512 words at ES:0, abort early if POST_SKIP (CF=1). */
.ptmf_write_only:
    xor di, di
    mov cx, 512
.ptmf_wr_chunk:
    push cx
    mov cx, 64
    rep stosw
    pop cx
    sub cx, 64
    push ds
    push bx
    xor bx, bx
    mov ds, bx
    cmp byte ptr [POST_SKIP], 0
    pop bx
    pop ds
    jne .ptmf_wr_esc
    test cx, cx
    jnz .ptmf_wr_chunk
    clc
    ret
.ptmf_wr_esc:
    stc
    ret

/*
 * In-place "\rNNNNN KB" with 5-digit space pad so the count-up overwrites cleanly.
 * AX = KB.
 */
post_put_kb_line:
    push ax
    push bx
    push cx
    push dx
    push si
    push ds

    push ax
    push cs
    pop ds
    mov si, offset msg_cr_only
    call post_puts
    pop ax

    mov bx, 10
    xor cx, cx
    test ax, ax
    jnz .pkb_div
    xor dx, dx
    push dx
    mov cx, 1
    jmp .pkb_pad
.pkb_div:
    xor dx, dx
    div bx
    push dx
    inc cx
    test ax, ax
    jnz .pkb_div
.pkb_pad:
    mov dx, 5
    sub dx, cx
    jz .pkb_digits
.pkb_sp:
    mov ah, 0x0E
    mov al, ' '
    mov bh, 0
    int 0x10
    dec dx
    jnz .pkb_sp
.pkb_digits:
    pop dx
    mov ah, 0x0E
    mov al, dl
    add al, '0'
    mov bh, 0
    int 0x10
    loop .pkb_digits

    push cs
    pop ds
    mov si, offset msg_kb_suffix
    call post_puts

    pop ds
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* Equipment: note missing IPL floppy as 601 (non-fatal style report). */
post_test_equipment:
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    test word ptr [BDA_EQUIP], 0x0001
    jnz .pte_ok
    mov ax, 601
    call post_report_error
.pte_ok:
    pop ds
    ret

/* One short beep via PIT2 + PPI speaker gate. */
post_beep_ok:
    push ax
    push cx
    push dx
    mov al, 0xB6
    out PORT_PIT_MODE, al
    mov ax, 0x0533
    out 0x42, al
    mov al, ah
    out 0x42, al
    in al, PORT_PPI_B
    mov ah, al
    or al, 0x03
    out PORT_PPI_B, al
    mov cx, 0x3000
.pbo_delay:
    loop .pbo_delay
    mov al, ah
    and al, 0xFC
    out PORT_PPI_B, al
    pop dx
    pop cx
    pop ax
    ret

post_banner:
    .asciz "rmDOS BIOS\r\n\r\n"

msg_post_hint:
    .asciz "POST: ESC skips diagnostics\r\n"

msg_post_ok:
    .asciz "\r\nSystem OK\r\n"

msg_post_skip:
    .asciz "\r\nPOST skipped\r\n"

msg_error_f1:
    .asciz "\r\nERROR (RESUME = F1 KEY)\r\n"

msg_crlf:
    .asciz "\r\n"

msg_cr_only:
    .asciz "\r"

msg_kb_suffix:
    .asciz " KB"

msg_kb_ok:
    .asciz " OK\r\n"
