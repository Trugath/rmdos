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
    /* Ctrl-Break (BIOS INT 1Bh) → INT 23h when BREAK ON */
    lea bx, [int1b_dos]
    mov word ptr es:[0x1B * 4], bx
    mov word ptr es:[0x1B * 4 + 2], cs
    call install_absdisk_vectors
    call install_int2f_vector
    pop es
    pop bx
    pop ax
    ret

int20_handler:
    xor al, al
    jmp dos_terminate

int23_default:
    /* Ctrl-C / Ctrl-Break: abort current process */
    mov al, 0xC3
    jmp dos_terminate

/*
 * BIOS INT 1Bh (Ctrl-Break). When break_flag is set, invoke INT 23h
 * (same abort path as ^C). When off, IRET only — BDA latch is already set.
 */
int1b_dos:
    push ax
    cmp byte ptr cs:[break_flag], 0
    je .i1b_done
    int 0x23
.i1b_done:
    pop ax
    iret

int24_default:
    /* Critical error: Abort / Retry / Ignore via BIOS console */
    push bx
    push cx
    push dx
    push si
    push ds
    push cs
    pop ds
    lea si, [msg_int24]
.i24_puts:
    lodsb
    test al, al
    jz .i24_key
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    jmp .i24_puts
.i24_key:
    mov ah, 0x01
    int 0x16
    jz .i24_fail_quiet          /* no key — Fail (headless / noninteractive) */
    xor ah, ah
    int 0x16
    call .i24_up
    cmp al, 'A'
    je .i24_abort
    cmp al, 'R'
    je .i24_retry
    cmp al, 'I'
    je .i24_ignore
    /* unrecognized key → Fail (do not spin forever) */
    jmp .i24_fail_quiet
.i24_fail_quiet:
    mov al, 3
    jmp .i24_done
.i24_abort:
    mov al, 2
    jmp .i24_done
.i24_retry:
    mov al, 1
    jmp .i24_done
.i24_ignore:
    xor al, al
.i24_done:
    pop ds
    pop si
    pop dx
    pop cx
    pop bx
    iret
.i24_up:
    cmp al, 'a'
    jb .i24_up_done
    cmp al, 'z'
    ja .i24_up_done
    sub al, 0x20
.i24_up_done:
    ret

/*
 * Invoke INT 24h for a floppy I/O failure.
 * IN:  BH = 0 read / 1 write (mapped into AH)
 * OUT: AL = 0 ignore, 1 retry, 2 abort, 3 fail
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
    inc byte ptr cs:[dos_indos]
    cmp ah, 0x00
    je dos_ah00
    cmp ah, 0x01
    je dos_ah01
    cmp ah, 0x02
    je dos_ah02
    cmp ah, 0x03
    je dos_ah03
    cmp ah, 0x04
    je dos_ah04
    cmp ah, 0x05
    je dos_ah05
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
    cmp ah, 0x0C
    je dos_ah0c
    cmp ah, 0x0D
    je dos_ah0d
    cmp ah, 0x0E
    je dos_ah0e
    cmp ah, 0x0F
    je dos_ah0f
    cmp ah, 0x10
    je dos_ah10
    cmp ah, 0x11
    je dos_ah11
    cmp ah, 0x12
    je dos_ah12
    cmp ah, 0x13
    je dos_ah13
    cmp ah, 0x14
    je dos_ah14
    cmp ah, 0x15
    je dos_ah15
    cmp ah, 0x16
    je dos_ah16
    cmp ah, 0x17
    je dos_ah17
    cmp ah, 0x19
    je dos_ah19
    cmp ah, 0x1A
    je dos_ah1a
    cmp ah, 0x1B
    je dos_ah1b
    cmp ah, 0x1C
    je dos_ah1c
    cmp ah, 0x1F
    je dos_ah1f
    cmp ah, 0x21
    je dos_ah21
    cmp ah, 0x22
    je dos_ah22
    cmp ah, 0x23
    je dos_ah23
    cmp ah, 0x24
    je dos_ah24
    cmp ah, 0x25
    je dos_ah25
    cmp ah, 0x26
    je dos_ah26
    cmp ah, 0x27
    je dos_ah27
    cmp ah, 0x28
    je dos_ah28
    cmp ah, 0x29
    je dos_ah29
    cmp ah, 0x2A
    je dos_ah2a
    cmp ah, 0x2B
    je dos_ah2b
    cmp ah, 0x2C
    je dos_ah2c
    cmp ah, 0x2D
    je dos_ah2d
    cmp ah, 0x2E
    je dos_ah2e
    cmp ah, 0x2F
    je dos_ah2f
    cmp ah, 0x30
    je dos_ah30
    cmp ah, 0x31
    je dos_ah31
    cmp ah, 0x32
    je dos_ah32
    cmp ah, 0x33
    je dos_ah33
    cmp ah, 0x34
    je dos_ah34
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
    cmp ah, 0x45
    je dos_ah45
    cmp ah, 0x46
    je dos_ah46
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
    cmp ah, 0x50
    je dos_ah50
    cmp ah, 0x51
    je dos_ah51
    cmp ah, 0x52
    je dos_ah52
    cmp ah, 0x54
    je dos_ah54
    cmp ah, 0x55
    je dos_ah55
    cmp ah, 0x56
    je dos_ah56
    cmp ah, 0x57
    je dos_ah57
    cmp ah, 0x58
    je dos_ah58
    cmp ah, 0x59
    je dos_ah59
    cmp ah, 0x5A
    je dos_ah5a
    cmp ah, 0x5B
    je dos_ah5b
    cmp ah, 0x5C
    je dos_ah5c
    cmp ah, 0x60
    je dos_ah60
    cmp ah, 0x62
    je dos_ah62
    cmp ah, 0x65
    je dos_ah65
    cmp ah, 0x66
    je dos_ah66
    cmp ah, 0x67
    je dos_ah67
    cmp ah, 0x68
    je dos_ah68
    mov ax, 1
    stc
    jmp int21_iret_cf

/* AH=00h — terminate with AL=exit code (same as INT 20h path) */
dos_ah00:
    jmp dos_terminate

/* AH=0Dh — disk reset / flush buffers */
dos_ah0d:
    call fat12_flush_fat
    call dos_rebuild_drivemap
    clc
    jmp int21_iret

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

/* AH=31h � terminate and stay resident: AL=code, DX=paragraphs to keep */
dos_ah31:
    jmp dos_tsr

/* AH=4Dh — get child exit code */
dos_ah4d:
    mov al, byte ptr cs:[child_exit_code]
    mov ah, byte ptr cs:[child_exit_type]
    clc
    jmp int21_iret

dos_terminate:
    mov byte ptr cs:[dos_indos], 0
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

dos_tsr:
    mov byte ptr cs:[child_exit_code], al
    mov byte ptr cs:[child_exit_type], 3
    cmp byte ptr cs:[com_active], 0
    jne .a31_tsr_resume
    push es
    push bx
    mov es, word ptr cs:[current_psp]
    mov bx, dx
    mov ah, 0x4A
    int 0x21
    pop bx
    pop es
    clc
    jmp int21_iret
.a31_tsr_resume:
    mov byte ptr cs:[dos_indos], 0
    jmp com_tsr_kernel

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

/* AH=0Eh — select drive DL; AL = configured LASTDRIVE count */
dos_ah0e:
    push dx
    call dos_rebuild_drivemap
    call dos_select_drive
    pop dx
    mov al, byte ptr cs:[cfg_lastdrive]
    cmp al, CDS_COUNT
    jbe .a0e_count_ok
    mov al, CDS_COUNT
.a0e_count_ok:
    clc
    jmp int21_iret

/*
 * Mount logical drive DL (0=A) for filesystem I/O without changing cur_drive
 * or cwd. Rebuilds the map if the letter is not yet known (e.g. after
 * PARTEDIT/FORMAT). CF if unavailable.
 */
dos_mount_drive:
    push ax
    push bx
    push dx
    push ds
    push cs
    pop ds
    cmp dl, byte ptr [num_drives]
    jb .dmd_have
    push dx
    call dos_rebuild_drivemap
    pop dx
    cmp dl, byte ptr [num_drives]
    jae .dmd_bad
.dmd_have:
    mov bl, dl
    xor bh, bh
    mov al, [drive_map_bios + bx]
    cmp al, [boot_drive]
    jne .dmd_switch
    add bx, bx
    mov ax, [drive_map_base + bx]
    cmp ax, [vol_want_base]
    je .dmd_ok
    mov bl, dl
.dmd_switch:
    call fat12_flush_fat
    xor bh, bh
    mov bl, dl
    mov al, [drive_map_bios + bx]
    mov [boot_drive], al
    add bx, bx
    mov ax, [drive_map_base + bx]
    mov [vol_want_base], ax
    call fat12_init_bpb
    jc .dmd_bad
    call fat12_load_fat
    jc .dmd_bad
.dmd_ok:
    clc
    jmp .dmd_done
.dmd_bad:
    stc
.dmd_done:
    pop ds
    pop dx
    pop bx
    pop ax
    ret

/* Remount cur_drive volume (after a path op on another letter). */
dos_remount_current:
    push dx
    mov dl, byte ptr cs:[cur_drive]
    call dos_mount_drive
    pop dx
    ret

/*
 * Select logical drive DL (0=A): mount it and make it current (AH=0Eh).
 * Saves/restores per-drive cwd. CF if unavailable.
 */
dos_select_drive:
    push ax
    push bx
    push cx
    push si
    push di
    push es
    push ds
    push dx
    push cs
    pop ds
    push cs
    pop es
    /* Save cwd of current drive into per-drive slot */
    mov bl, [cur_drive]
    xor bh, bh
    cmp bl, byte ptr [cfg_lastdrive]
    jae .dsd_mount
    mov ax, [cwd_cluster]
    add bx, bx
    mov [drive_cwd_cluster + bx], ax
    mov bl, [cur_drive]
    xor bh, bh
    mov ax, bx
    mov cl, 6
    shl ax, cl                   /* *64 */
    lea di, [drive_cwd_path]
    add di, ax
    lea si, [cwd_path]
    mov cx, 64
    rep movsb
.dsd_mount:
    pop dx
    push dx
    call dos_mount_drive
    jc .dsd_bad
    mov byte ptr [cur_drive], dl
    /* Restore cwd for new drive */
    mov bl, dl
    xor bh, bh
    cmp bl, byte ptr [cfg_lastdrive]
    jae .dsd_root
    mov si, bx
    add si, si
    mov ax, [drive_cwd_cluster + si]
    mov [cwd_cluster], ax
    mov ax, bx
    mov cl, 6
    shl ax, cl
    lea si, [drive_cwd_path]
    add si, ax
    lea di, [cwd_path]
    mov cx, 64
    rep movsb
    jmp .dsd_ok
.dsd_root:
    mov word ptr [cwd_cluster], 0
    mov byte ptr [cwd_path], 0
.dsd_ok:
    call dos_sysvars_refresh
    clc
    jmp .dsd_done
.dsd_bad:
    stc
.dsd_done:
    pop dx
    pop ds
    pop es
    pop di
    pop si
    pop cx
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

/* AH=2Ch — get time → CH:CL:DH:DL from soft time or BIOS ticks */
dos_ah2c:
    cmp byte ptr cs:[dos_time_set], 0
    je .a2c_ticks
    mov ch, byte ptr cs:[dos_hour]
    mov cl, byte ptr cs:[dos_minute]
    mov dh, byte ptr cs:[dos_second]
    mov dl, byte ptr cs:[dos_hsecond]
    clc
    jmp int21_iret

.a2c_ticks:
    push ax
    push bx
    push si
    push ds
    mov ax, 0x40
    mov ds, ax
    mov ax, [0x6C]
    mov dx, [0x6E]
    pop ds
    /* Use 18 ticks/second. Divide into hours first so DIV cannot overflow. */
    mov bx, 64800
    div bx
    cmp ax, 24
    jb .a2c_hour_ok
    sub ax, 24
.a2c_hour_ok:
    mov ch, al
    mov ax, dx
    xor dx, dx
    mov bx, 1080
    div bx
    mov cl, al
    mov ax, dx
    xor dx, dx
    mov bx, 18
    div bx
    mov dh, al
    mov si, dx
    mov ax, si
    mov bx, 100
    mul bx
    mov bx, 18
    div bx
    mov dl, al
    pop si
    pop bx
    pop ax
    clc
    jmp int21_iret

/* AH=2Dh — set soft time CH:CL:DH:DL (no RTC required) */
dos_ah2d:
    cmp ch, 23
    ja .a2d_bad
    cmp cl, 59
    ja .a2d_bad
    cmp dh, 59
    ja .a2d_bad
    cmp dl, 99
    ja .a2d_bad
    mov byte ptr cs:[dos_hour], ch
    mov byte ptr cs:[dos_minute], cl
    mov byte ptr cs:[dos_second], dh
    mov byte ptr cs:[dos_hsecond], dl
    mov byte ptr cs:[dos_time_set], 1
    xor al, al
    clc
    jmp int21_iret
.a2d_bad:
    mov al, 0xFF
    clc
    jmp int21_iret

/* AH=2Eh — verify write flag AL=0 off / 1 on (writes re-verify via INT 13 AH=04) */
dos_ah2e:
    mov byte ptr cs:[dos_verify_flag], al
    xor al, al
    clc
    jmp int21_iret

/* AH=54h — get verify flag → AL */
dos_ah54:
    mov al, byte ptr cs:[dos_verify_flag]
    clc
    jmp int21_iret

/* AH=34h — get InDOS flag address → ES:BX */
dos_ah34:
    push cs
    pop es
    lea bx, [dos_indos]
    clc
    jmp int21_iret

/* DL = logical drive index (0=A) → BX = its CS:dos_dpb slot. */
dpb_ptr_for_drive:
    push ax
    mov al, dl
    mov bl, DPB_SIZE
    mul bl
    lea bx, [dos_dpb]
    add bx, ax
    pop ax
    ret

/*
 * Fill the per-drive DPB slot from the currently mounted BPB.
 * DL = logical drive index (0=A). Clobbers AX,BX.
 */
dpb_fill_from_bpb:
    push di
    call dpb_ptr_for_drive
    mov di, bx
    mov al, dl
    mov byte ptr cs:[di], al                /* +00 drive 0=A */
    mov byte ptr cs:[di + 1], al            /* +01 unit */
    mov word ptr cs:[di + 2], 512           /* +02 bytes/sector */
    mov al, byte ptr cs:[bpb_spc]
    dec al
    mov byte ptr cs:[di + 4], al            /* +04 spc-1 */
    mov al, byte ptr cs:[bpb_spc]
    xor ah, ah
    xor bx, bx                              /* cluster → sector shift */
.dpb_sh:
    cmp ax, 1
    jbe .dpb_sh_done
    shr ax, 1
    inc bx
    jmp .dpb_sh
.dpb_sh_done:
    mov byte ptr cs:[di + 5], bl
    mov ax, word ptr cs:[bpb_reserved]
    mov word ptr cs:[di + 6], ax            /* +06 reserved */
    mov al, byte ptr cs:[bpb_fats]
    mov byte ptr cs:[di + 8], al
    mov ax, word ptr cs:[bpb_root_ents]
    mov word ptr cs:[di + 9], ax
    mov ax, word ptr cs:[bpb_data_lba]
    sub ax, word ptr cs:[vol_want_base]
    mov word ptr cs:[di + 0x0B], ax         /* +0B first data sector */
    mov ax, word ptr cs:[bpb_max_clust]
    mov word ptr cs:[di + 0x0D], ax         /* +0D max cluster */
    mov ax, word ptr cs:[bpb_spf]
    mov word ptr cs:[di + 0x0F], ax
    mov ax, word ptr cs:[bpb_root_lba]
    sub ax, word ptr cs:[vol_want_base]
    mov word ptr cs:[di + 0x11], ax         /* +11 first dir sector */
    lea ax, [dev_nul_hdr]
    mov word ptr cs:[di + 0x13], ax         /* +13 device header off */
    mov word ptr cs:[di + 0x15], cs         /* +15 device header seg */
    mov al, byte ptr cs:[bpb_media]
    mov byte ptr cs:[di + 0x17], al
    mov byte ptr cs:[di + 0x18], 0xFF       /* not accessed */
    mov word ptr cs:[di + 0x19], 0
    mov word ptr cs:[di + 0x1B], 0
    mov word ptr cs:[di + 0x1D], 0
    mov word ptr cs:[di + 0x1F], 0xFFFF
    pop di
    ret

/*
 * Resolve AH=32/1C drive: DL=0 default → cur; else DL=1=A → index 0.
 * On success DL=0-based index, CF clear. CF set if unmapped.
 */
dos_resolve_drive_dl:
    push ax
    mov al, dl
    test al, al
    jnz .drd_num
    mov dl, byte ptr cs:[cur_drive]
    jmp .drd_chk
.drd_num:
    dec al
    mov dl, al
.drd_chk:
    cmp dl, byte ptr cs:[num_drives]
    jae .drd_bad
    clc
    pop ax
    ret
.drd_bad:
    stc
    pop ax
    ret

/* AH=1Fh — get DPB for default drive → DS:BX; AL=FF on bad */
dos_ah1f:
    mov dl, byte ptr cs:[cur_drive]
    call dpb_fill_from_bpb
    call dpb_ptr_for_drive
    push cs
    pop ds
    xor al, al
    clc
    jmp int21_iret

/* AH=32h — get DPB for DL (0=default, 1=A…) → DS:BX; AL=FF on bad drive */
dos_ah32:
    push dx
    call dos_resolve_drive_dl
    jc .a32_bad
    call dos_mount_drive
    jc .a32_bad_m
    call dpb_fill_from_bpb
    call dpb_ptr_for_drive
    call dos_remount_current
    pop dx
    push cs
    pop ds
    xor al, al
    clc
    jmp int21_iret
.a32_bad_m:
    call dos_remount_current
.a32_bad:
    pop dx
    mov al, 0xFF
    clc
    jmp int21_iret

/* AH=58h — get/set alloc strategy: AL=0 get BX, AL=1 set BX */
dos_ah58:
    cmp al, 0
    je .a58_get
    cmp al, 1
    je .a58_set
    mov ax, 1
    stc
    jmp int21_iret_cf
.a58_get:
    mov bx, word ptr cs:[dos_alloc_strategy]
    xor ax, ax
    clc
    jmp int21_iret
.a58_set:
    mov word ptr cs:[dos_alloc_strategy], bx
    xor ax, ax
    clc
    jmp int21_iret

/* AH=67h — get/set handle count: AL=0 get BX, AL=1 set BX */
dos_ah67:
    cmp al, 0
    je .a67_get
    cmp al, 1
    je .a67_set
    mov ax, 1
    stc
    jmp int21_iret_cf
.a67_get:
    mov bx, word ptr cs:[max_handles]
    xor ax, ax
    clc
    jmp int21_iret
.a67_set:
    call handles_set_max
    jc .a67_fail
    xor ax, ax
    clc
    jmp int21_iret
.a67_fail:
    mov ax, 8
    stc
    jmp int21_iret_cf

/* AH=68h — commit file BX (flush dirent/FAT for files; devices OK) */
dos_ah68:
    push di
    push es
    push cs
    pop es
    call handle_index
    jc .a68_err
    call handle_flush_file
    pop es
    pop di
    xor ax, ax
    clc
    jmp int21_iret
.a68_err:
    pop es
    pop di
    mov ax, 6
    stc
    jmp int21_iret_cf

/* AH=36h — AX=spc, BX=free clusters, CX=bps, DX=total clusters */
dos_ah36:
    push si
    push di
    push ds
    push cs
    pop ds
    call dos_resolve_drive_dl
    jc .a36_fail
    call dos_mount_drive
    jc .a36_fail_m
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
    mov bx, si
    mov dx, word ptr cs:[bpb_max_clust]
    cmp dx, 2
    jb .a36_total_done
    sub dx, 2
.a36_total_done:
    mov ax, cs:[bpb_spc]
    xor ah, ah
    mov cx, 512
    call dos_remount_current
    pop ds
    pop di
    pop si
    clc
    jmp int21_iret
.a36_fail_m:
    call dos_remount_current
.a36_fail:
    pop ds
    pop di
    pop si
    mov ax, 0xFFFF
    clc
    jmp int21_iret

/* AH=1Bh — allocation info for default drive; AH=1Ch for DL (0=default,1=A) */
dos_ah1b:
    xor dl, dl
    call dos_alloc_info
    clc
    jmp int21_iret

dos_ah1c:
    call dos_alloc_info
    clc
    jmp int21_iret

/* DL=0 default / 1=A… → AL=spc, CX=512, DX=total clusters, DS:BX→media */
dos_alloc_info:
    push si
    push di
    call dos_resolve_drive_dl
    jc .dai_fail
    call dos_mount_drive
    jc .dai_fail_m
    mov al, byte ptr cs:[bpb_spc]
    mov cx, 512
    mov dx, word ptr cs:[bpb_max_clust]
    cmp dx, 2
    jb .dai_dx
    dec dx
.dai_dx:
    push cs
    pop ds
    lea bx, [bpb_media]
    call dos_remount_current
    pop di
    pop si
    clc
    ret
.dai_fail_m:
    call dos_remount_current
.dai_fail:
    mov al, byte ptr cs:[bpb_spc]
    mov cx, 512
    mov dx, word ptr cs:[bpb_max_clust]
    cmp dx, 2
    jb .dai_fdx
    dec dx
.dai_fdx:
    push cs
    pop ds
    lea bx, [bpb_media]
    pop di
    pop si
    stc
    ret

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

/* AH=38h — get/set country info */
dos_ah38:
    cmp al, 0xFF
    je .a38_set
    cmp dx, 0xFFFF
    je .a38_code_only
    push cx
    push si
    push di
    push es
    push ds
    push ds
    pop es
    mov di, dx
    push cs
    pop ds
    lea si, [country_info]
    mov cx, 34
    rep movsb
    pop ds
    pop es
    pop di
    pop si
    pop cx
.a38_code_only:
    mov bx, word ptr cs:[dos_country_id]
    xor ax, ax
    clc
    jmp int21_iret
.a38_set:
    mov word ptr cs:[dos_country_id], bx
    xor ax, ax
    clc
    jmp int21_iret

/* AH=50h — set current PSP = BX */
dos_ah50:
    mov word ptr cs:[current_psp], bx
    clc
    jmp int21_iret

/* AH=51h — get current PSP -> BX */
dos_ah51:
    mov bx, word ptr cs:[current_psp]
    clc
    jmp int21_iret

/* AH=62h — get PSP (alias of 51h) */
dos_ah62:
    jmp dos_ah51

/* AH=65h — get extended country info; AL=01 → ES:DI extended + country_info */
dos_ah65:
    cmp al, 1
    je .a65_get
    cmp al, 2
    je .a65_ucase
    mov ax, 1
    stc
    jmp int21_iret_cf
.a65_get:
    push ax
    push bx
    push cx
    push si
    push di
    push ds
    /* Extended header */
    mov byte ptr es:[di], 1
    mov word ptr es:[di + 1], 34
    mov ax, word ptr cs:[dos_country_id]
    mov word ptr es:[di + 3], ax
    mov ax, word ptr cs:[dos_codepage_active]
    mov word ptr es:[di + 5], ax
    add di, 7
    push cs
    pop ds
    lea si, [country_info]
    mov cx, 34
    rep movsb
    pop ds
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    xor ax, ax
    clc
    jmp int21_iret
.a65_ucase:
    /* AL=02 — return far pointer to uppercase/case-map table in DX:BX */
    lea bx, [country_case_map]
    push cs
    pop dx
    xor ax, ax
    clc
    jmp int21_iret

/* AH=66h — get/set global code page (DOS 3.3-ish). */
dos_ah66:
    cmp al, 1
    je .a66_get
    cmp al, 2
    je .a66_set
    mov ax, 1
    stc
    jmp int21_iret_cf
.a66_get:
    mov bx, word ptr cs:[dos_codepage_active]
    mov dx, word ptr cs:[dos_codepage_system]
    xor ax, ax
    clc
    jmp int21_iret
.a66_set:
    mov word ptr cs:[dos_codepage_active], bx
    mov word ptr cs:[dos_codepage_system], bx
    xor ax, ax
    clc
    jmp int21_iret

/* AH=52h — get DOS list of lists -> ES:BX */
dos_ah52:
    push ax
    push cx
    push si
    push di
    push ds
    call dos_sysvars_refresh
    pop ds
    pop di
    pop si
    pop cx
    pop ax
    push cs
    pop es
    lea bx, [dos_sysvars]
    clc
    jmp int21_iret

/*
 * Refresh LoL pointers, SFT count, and CDS paths for mapped drives.
 */
dos_sysvars_refresh:
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
    push cs
    pop es

    mov ax, word ptr [first_mcb]
    mov word ptr [dos_sysvars], ax
    mov ax, cs
    mov word ptr [dos_sysvars + 6], ax
    mov word ptr [dos_sysvars + 0x0A], ax
    mov word ptr [dos_sysvars + 0x0E], ax
    mov word ptr [dos_sysvars + 0x18], ax
    mov ax, word ptr [max_handles]
    mov word ptr [dos_sft_header + 4], ax
    call sft_refresh_table
    call dos_buffers_refresh
    mov al, byte ptr [num_drives]
    mov byte ptr [dos_sysvars + 0x20], al
    mov al, byte ptr [cfg_lastdrive]
    mov byte ptr [dos_sysvars + 0x21], al
    mov al, byte ptr [cur_drive]
    mov byte ptr [dos_sysvars + 0x22], al
    call dos_cwd_save_current

    /* CDS[i] = "X:" + cwd; SUBST flag when active */
    xor bx, bx
.svr_cds:
    cmp bl, byte ptr [cfg_lastdrive]
    jae .svr_done
    /* clear entry */
    mov ax, bx
    mov cl, CDS_ENTRY_SIZE
    mul cl
    lea di, [dos_cds]
    add di, ax
    mov cx, CDS_ENTRY_SIZE
    xor al, al
    rep stosb

    mov al, 0
    cmp bl, byte ptr [num_drives]
    jb .svr_cds_use
    cmp byte ptr [subst_real + bx], 0xFF
    je .svr_cds_next
.svr_cds_use:
    mov ax, bx
    mov cl, CDS_ENTRY_SIZE
    mul cl
    lea di, [dos_cds]
    add di, ax
    mov al, bl
    add al, 'A'
    mov byte ptr [di], al
    mov byte ptr [di + 1], ':'
    mov byte ptr [di + 2], '\\'
    mov byte ptr [di + 3], 0
    /* copy drive_cwd_path after X: if non-empty */
    push bx
    mov ax, bx
    mov cl, 6
    shl ax, cl
    lea si, [drive_cwd_path]
    add si, ax
    cmp byte ptr [si], 0
    je .svr_cds_no_cwd
    lea di, [di + 2]
    cmp byte ptr [si], '\\'
    jne .svr_cds_cp_loop
    inc si
.svr_cds_cp_loop:
    lodsb
    stosb
    test al, al
    jnz .svr_cds_cp_loop
.svr_cds_no_cwd:
    pop bx
    mov ax, bx
    mov cl, CDS_ENTRY_SIZE
    mul cl
    lea di, [dos_cds]
    add di, ax
    mov word ptr [di + 0x43], 0x4000
    cmp byte ptr [subst_real + bx], 0xFF
    je .svr_cds_dpb
    or word ptr [di + 0x43], 0x1000
.svr_cds_dpb:
    mov ax, bx
    mov cl, DPB_SIZE
    mul cl
    lea si, [dos_dpb]
    add ax, si
    mov word ptr [di + 0x45], ax
    mov word ptr [di + 0x47], cs
.svr_cds_next:
    inc bx
    jmp .svr_cds
.svr_done:
    mov al, byte ptr [cfg_lastdrive]
    mov byte ptr [dos_sysvars + 0x21], al
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
 * Link cfg_buffers DOS buffer headers and point LoL +12 at the first.
 * Headers are free sentinels (drive=FFh); FAT I/O stays on the windowed cache.
 */
dos_buffers_refresh:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push cs
    pop ds

    mov cx, word ptr [cfg_buffers]
    cmp cx, 1
    jae .db_lo
    mov cx, 1
.db_lo:
    cmp cx, DOS_BUF_CAP
    jbe .db_hi
    mov cx, DOS_BUF_CAP
.db_hi:
    lea di, [dos_buf_table]
    mov word ptr [dos_sysvars + 0x12], di
    mov word ptr [dos_sysvars + 0x14], cs
    xor bx, bx
.db_loop:
    cmp bx, cx
    jae .db_done
    mov ax, bx
    mov dl, DOS_BUF_HDR_SIZE
    mul dl
    lea di, [dos_buf_table]
    add di, ax
    /* clear */
    push cx
    push di
    mov cx, DOS_BUF_HDR_SIZE
    xor al, al
    rep stosb
    pop di
    pop cx
    mov byte ptr [di + 4], 0xFF  /* free */
    mov ax, bx
    inc ax
    cmp ax, cx
    jae .db_term
    mov dl, DOS_BUF_HDR_SIZE
    mul dl
    lea si, [dos_buf_table]
    add ax, si
    mov word ptr [di], ax
    mov word ptr [di + 2], cs
    jmp .db_next
.db_term:
    mov word ptr [di], 0xFFFF
    mov word ptr [di + 2], 0xFFFF
.db_next:
    inc bx
    jmp .db_loop
.db_done:
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* AH=59h — get extended error info */
dos_ah59:
    mov ax, word ptr cs:[dos_last_error]
    mov bh, 8                    /* class: not found / unknown */
    mov bl, 3                    /* action: abort */
    mov ch, 2                    /* locus: block device */
    xor cl, cl
    clc
    jmp int21_iret

int21_iret:
    push bp
    mov bp, sp
    and word ptr [bp + 6], 0xFFFE
    pop bp
    dec byte ptr cs:[dos_indos]
    iret

int21_iret_cf:
    mov word ptr cs:[dos_last_error], ax
    push bp
    mov bp, sp
    or word ptr [bp + 6], 0x0001
    pop bp
    dec byte ptr cs:[dos_indos]
    iret

/*
 * Check Ctrl-C if break_flag set. Invokes INT 23h on ^C.
 * Call from console I/O. Preserves all except may not return if abort.
 */
check_break:
    push ax
    push bx
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
    pop bx
    pop ax
    ret
