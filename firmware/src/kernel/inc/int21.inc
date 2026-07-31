/*
 * INT 20h / INT 21h install + dispatch.
 */

install_dos_vectors:
    push ax
    push bx
    push es
    xor ax, ax
    mov es, ax
    lea bx, [int20_handler]
    mov word ptr es:[0x20 * 4], bx
    mov word ptr es:[0x20 * 4 + 2], cs
    lea bx, [int21_handler]
    mov word ptr es:[0x21 * 4], bx
    mov word ptr es:[0x21 * 4 + 2], cs
    /* default INT 23h / INT 24h */
    lea bx, [int23_default]
    mov word ptr es:[0x23 * 4], bx
    mov word ptr es:[0x23 * 4 + 2], cs
    lea bx, [int24_default]
    mov word ptr es:[0x24 * 4], bx
    mov word ptr es:[0x24 * 4 + 2], cs
    pop es
    pop bx
    pop ax
    ret

int20_handler:
    xor al, al
    jmp dos_terminate

int23_default:
    /* ignore Ctrl-C: return without abort */
    iret

int24_default:
    /* critical error: fail (AL=3) */
    mov al, 3
    iret

/*
 * Invoke INT 24h for a floppy I/O failure.
 * IN:  BH = 0 read / 1 write (mapped into AH)
 * OUT: AL = 0 retry, 1 ignore, 2 abort, 3 fail (default handler → 3)
 * Preserves all other GPRs except AX/flags.
 */
invoke_int24_disk:
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es
    mov ah, 0x02                 /* disk error, read */
    test bh, 1
    jz .i24_ah
    mov ah, 0x0A                 /* disk error, write */
.i24_ah:
    mov al, byte ptr cs:[cur_drive]
    xor bx, bx                   /* BPB unused */
    mov cx, 1                    /* sector count hint */
    mov di, 0x000B               /* read fault / general failure */
    int 0x24
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ret

int21_handler:
    sti
    cmp ah, 0x01
    je dos_ah01
    cmp ah, 0x02
    je dos_ah02
    cmp ah, 0x06
    je dos_ah06
    cmp ah, 0x07
    je dos_ah07
    cmp ah, 0x08
    je dos_ah08
    cmp ah, 0x09
    je dos_ah09
    cmp ah, 0x0A
    je dos_ah0a
    cmp ah, 0x0B
    je dos_ah0b
    cmp ah, 0x0E
    je dos_ah0e
    cmp ah, 0x19
    je dos_ah19
    cmp ah, 0x1A
    je dos_ah1a
    cmp ah, 0x25
    je dos_ah25
    cmp ah, 0x2A
    je dos_ah2a
    cmp ah, 0x2B
    je dos_ah2b
    cmp ah, 0x2C
    je dos_ah2c
    cmp ah, 0x2D
    je dos_ah2d
    cmp ah, 0x30
    je dos_ah30
    cmp ah, 0x33
    je dos_ah33
    cmp ah, 0x35
    je dos_ah35
    cmp ah, 0x36
    je dos_ah36
    cmp ah, 0x37
    je dos_ah37
    cmp ah, 0x38
    je dos_ah38
    cmp ah, 0x39
    je dos_ah39
    cmp ah, 0x3A
    je dos_ah3a
    cmp ah, 0x3B
    je dos_ah3b
    cmp ah, 0x3C
    je dos_ah3c
    cmp ah, 0x3D
    je dos_ah3d
    cmp ah, 0x3E
    je dos_ah3e
    cmp ah, 0x3F
    je dos_ah3f
    cmp ah, 0x40
    je dos_ah40
    cmp ah, 0x41
    je dos_ah41
    cmp ah, 0x42
    je dos_ah42
    cmp ah, 0x43
    je dos_ah43
    cmp ah, 0x44
    je dos_ah44
    cmp ah, 0x47
    je dos_ah47
    cmp ah, 0x48
    je dos_ah48
    cmp ah, 0x49
    je dos_ah49
    cmp ah, 0x4A
    je dos_ah4a
    cmp ah, 0x4B
    je dos_ah4b
    cmp ah, 0x4C
    je dos_ah4c
    cmp ah, 0x4D
    je dos_ah4d
    cmp ah, 0x4E
    je dos_ah4e
    cmp ah, 0x4F
    je dos_ah4f
    cmp ah, 0x56
    je dos_ah56
    mov ax, 1
    stc
    jmp int21_iret_cf

/* AH=30h — DOS version 3.31 */
dos_ah30:
    mov ax, 0x1F03              /* AL=3 major, AH=31 minor */
    mov bx, 0x0000              /* BH=OEM 0, BL=0 */
    mov cx, 0
    clc
    jmp int21_iret

/* AH=4Ch terminate with AL=exit code */
dos_ah4c:
    jmp dos_terminate

/* AH=4Dh — get child exit code */
dos_ah4d:
    mov al, byte ptr cs:[child_exit_code]
    mov ah, byte ptr cs:[child_exit_type]
    clc
    jmp int21_iret

dos_terminate:
    mov byte ptr cs:[child_exit_code], al
    mov byte ptr cs:[child_exit_type], 0
    cmp byte ptr cs:[com_active], 0
    jne com_resume_kernel
    mov al, 0x0D
    call putch
    mov al, 0x0A
    call putch
.halt:
    hlt
    jmp .halt

/* AH=25h — set interrupt vector AL=number, DS:DX=handler */
dos_ah25:
    push bx
    push es
    pushf
    cli
    xor bx, bx
    mov es, bx
    mov bl, al
    xor bh, bh
    shl bx, 1
    shl bx, 1
    mov word ptr es:[bx], dx
    mov word ptr es:[bx + 2], ds
    popf
    pop es
    pop bx
    clc
    jmp int21_iret

/* AH=35h — get interrupt vector AL → ES:BX */
dos_ah35:
    push ax
    push ds
    mov bl, al
    xor bh, bh
    shl bx, 1
    shl bx, 1
    xor ax, ax
    mov ds, ax
    les bx, dword ptr [bx]
    pop ds
    pop ax
    clc
    jmp int21_iret

/* AH=33h — Ctrl-Break check flag: AL=0 get DL, AL=1 set DL */
dos_ah33:
    cmp al, 0
    je .a33_get
    cmp al, 1
    je .a33_set
    cmp al, 2
    je .a33_get
    xor al, al
    clc
    jmp int21_iret
.a33_get:
    mov dl, byte ptr cs:[break_flag]
    mov al, byte ptr cs:[break_flag]
    clc
    jmp int21_iret
.a33_set:
    mov byte ptr cs:[break_flag], dl
    clc
    jmp int21_iret

/* AH=19h — get current drive (0=A) */
dos_ah19:
    mov al, byte ptr cs:[cur_drive]
    clc
    jmp int21_iret

/* AH=0Eh — select drive DL; AL = number of logical drives */
dos_ah0e:
    push dx
    call dos_select_drive
    pop dx
    mov al, byte ptr cs:[num_drives]
    clc
    jmp int21_iret

/*
 * Select logical drive DL (0=A). Remounts volume. CF if unavailable.
 */
dos_select_drive:
    push ax
    push bx
    cmp dl, byte ptr cs:[num_drives]
    jae .dsd_bad
    cmp dl, byte ptr cs:[cur_drive]
    je .dsd_ok
    call fat12_flush_fat
    /* map logical → BIOS DL */
    mov bl, dl
    cmp bl, 2
    jb .dsd_bios
    mov bl, 0x80
    add bl, dl
    sub bl, 2                    /* C:→80h D:→81h */
.dsd_bios:
    mov byte ptr cs:[boot_drive], bl
    mov byte ptr cs:[cur_drive], dl
    call fat12_init_bpb
    jc .dsd_bad
    call fat12_load_fat
    jc .dsd_bad
    mov word ptr cs:[cwd_cluster], 0
    mov byte ptr cs:[cwd_path], 0
.dsd_ok:
    clc
    jmp .dsd_done
.dsd_bad:
    stc
.dsd_done:
    pop bx
    pop ax
    ret

/* AH=2Ah — get date → CX=year DH=month DL=day AL=dow */
dos_ah2a:
    mov cx, word ptr cs:[dos_year]
    mov dh, byte ptr cs:[dos_month]
    mov dl, byte ptr cs:[dos_day]
    mov al, byte ptr cs:[dos_dow]
    clc
    jmp int21_iret

/* AH=2Bh — set date CX/DH/DL */
dos_ah2b:
    cmp cx, 1980
    jb .a2b_bad
    cmp dh, 1
    jb .a2b_bad
    cmp dh, 12
    ja .a2b_bad
    cmp dl, 1
    jb .a2b_bad
    cmp dl, 31
    ja .a2b_bad
    mov word ptr cs:[dos_year], cx
    mov byte ptr cs:[dos_month], dh
    mov byte ptr cs:[dos_day], dl
    xor al, al
    clc
    jmp int21_iret
.a2b_bad:
    mov al, 0xFF
    clc
    jmp int21_iret

/* AH=2Ch — get time → CH:CL:DH:DL from BIOS ticks */
dos_ah2c:
    push ax
    push bx
    push si
    push ds
    mov ax, 0x40
    mov ds, ax
    mov ax, [0x6C]
    mov dx, [0x6E]
    pop ds
    /* ticks / 18 ≈ seconds */
    mov bx, 18
    div bx
    xor dx, dx
    mov bx, 60
    div bx                       /* DX=sec, AX=total minutes */
    mov si, dx                   /* SI = seconds */
    xor dx, dx
    div bx                       /* DX=min, AX=total hours */
    mov bx, dx                   /* BX = minutes */
    mov cl, 24
    div cl                       /* AH = hour of day */
    mov ch, ah
    mov cl, bl
    mov dx, si
    mov dh, dl
    xor dl, dl
    pop si
    pop bx
    pop ax
    clc
    jmp int21_iret

/* AH=2Dh — set time CH:CL:DH:DL (accept, no RTC) */
dos_ah2d:
    xor al, al
    clc
    jmp int21_iret

/* AH=36h — disk free space: DX=clusters free, AX=spc, BX=avail, CX=bps */
dos_ah36:
    push si
    push di
    push ds
    push cs
    pop ds
    /* count free FAT entries */
    xor si, si                   /* free count */
    mov di, 2
.a36_loop:
    cmp di, cs:[bpb_max_clust]
    jae .a36_done
    mov ax, di
    call fat12_get_fat
    test ax, ax
    jnz .a36_next
    inc si
.a36_next:
    inc di
    jmp .a36_loop
.a36_done:
    mov dx, si                   /* free clusters */
    mov bx, si                   /* available (same) */
    mov ax, cs:[bpb_spc]
    xor ah, ah
    mov cx, 512
    pop ds
    pop di
    pop si
    clc
    jmp int21_iret

/* AH=37h — switchar: AL=0 get DL='/', AL=1 set */
dos_ah37:
    cmp al, 0
    je .a37_get
    cmp al, 1
    je .a37_set
    mov al, 0xFF
    clc
    jmp int21_iret
.a37_get:
    mov dl, byte ptr cs:[switchar]
    xor al, al
    clc
    jmp int21_iret
.a37_set:
    mov byte ptr cs:[switchar], dl
    xor al, al
    clc
    jmp int21_iret

/* AH=38h — country info stub: copy defaults to DS:DX */
dos_ah38:
    push cx
    push si
    push di
    push es
    push ds
    pop es
    mov di, dx
    push cs
    pop ds
    lea si, [country_info]
    mov cx, 34
    rep movsb
    pop es
    pop di
    pop si
    pop cx
    mov bx, 1                    /* country code US */
    xor ax, ax
    clc
    jmp int21_iret

int21_iret:
    push bp
    mov bp, sp
    and word ptr [bp + 6], 0xFFFE
    pop bp
    iret

int21_iret_cf:
    push bp
    mov bp, sp
    or word ptr [bp + 6], 0x0001
    pop bp
    iret

/*
 * Check Ctrl-C if break_flag set. Invokes INT 23h on ^C.
 * Call from console I/O. Preserves all except may not return if abort.
 */
check_break:
    push ax
    cmp byte ptr cs:[break_flag], 0
    je .cb_done
    mov ah, 0x01
    int 0x16
    jz .cb_done
    cmp al, 0x03
    jne .cb_done
    /* consume key */
    mov ah, 0x00
    int 0x16
    int 0x23
.cb_done:
    pop ax
    ret
