.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * PARTEDIT — create/list primary and extended/logical DOS partitions on BIOS
 * drive 80h.
 * Usage:
 *   PARTEDIT /CREATE [/SIZE n] | /P | /AUTO | /LIST [C:]
 *   PARTEDIT /CREATEEXT [/SIZE n]
 *   PARTEDIT /CREATELOG [/SIZE n]
 * /CREATE adds the next free primary (start LBA 17 if empty). /SIZE sets
 * a 32-bit sector count; omit to use remaining disk space after track 0.
 * Disk size, partition start/length, and INT 13h LBA are dword (≤128 MiB).
 * /CREATEEXT adds a type-05 extended container; /CREATELOG adds a logical
 * DOS volume inside the first extended partition.
 */

_start:
    push cs
    pop ds
    push cs
    pop es
    mov byte ptr [drive], 0x80
    mov byte ptr [list_only], 0
    mov byte ptr [have_size], 0
    mov byte ptr [want_create], 0
    mov byte ptr [want_create_ext], 0
    mov byte ptr [want_create_log], 0
    mov word ptr [req_size], 0
    mov word ptr [req_size + 2], 0
    mov si, 0x81
    call parse_args
    jc usage
    cmp byte ptr [list_only], 0
    je .chk_create
    call do_list
    jc fail
    jmp ok_exit
.chk_create:
    mov al, [want_create]
    or al, [want_create_ext]
    or al, [want_create_log]
    jz usage
    call geometry
    jc fail
    cmp byte ptr [want_create_ext], 0
    je .chk_log
    call do_create_ext
    jc fail
    jmp ok_exit
.chk_log:
    cmp byte ptr [want_create_log], 0
    je .do_pri
    call do_create_log
    jc fail
    jmp ok_exit
.do_pri:
    call do_create
    jc fail
ok_exit:
    lea dx, [msg_ok]
    mov ah, 9
    int 0x21
    mov ax, 0x4C00
    int 0x21

usage:
    lea dx, [msg_usage]
    mov ah, 9
    int 0x21
    mov ax, 0x4C01
    int 0x21
fail:
    lea dx, [msg_fail]
    mov ah, 9
    int 0x21
    mov ax, 0x4C01
    int 0x21

parse_args:
.skip:
    mov al, [si]
    cmp al, ' '
    je .inc
    cmp al, 9
    je .inc
    cmp al, 0
    je .ok
    cmp al, 13
    je .ok
    cmp al, '/'
    je .sw
    cmp al, '-'
    je .sw
    cmp byte ptr [si + 1], ':'
    jne .bad
    mov al, [si]
    and al, 0xDF
    cmp al, 'C'
    jne .bad
    add si, 2
    jmp .skip
.inc:
    inc si
    jmp .skip
.sw:
    inc si
    mov al, [si]
    and al, 0xDF
    cmp al, 'P'
    jne .try_list
    mov byte ptr [want_create], 1
    inc si
    jmp .skip
.try_list:
    cmp al, 'L'
    jne .try_size
    cmp byte ptr [si + 1], 'I'
    jne .try_size
    cmp byte ptr [si + 2], 'S'
    jne .try_size
    cmp byte ptr [si + 3], 'T'
    jne .try_size
    mov byte ptr [list_only], 1
    add si, 4
    jmp .skip
.try_size:
    cmp al, 'S'
    jne .try_auto
    cmp byte ptr [si + 1], 'I'
    jne .try_auto
    cmp byte ptr [si + 2], 'Z'
    jne .try_auto
    cmp byte ptr [si + 3], 'E'
    jne .try_auto
    add si, 4
    call skip_sp
    call parse_dec
    jc .bad
    mov [req_size], ax
    mov [req_size + 2], dx
    mov byte ptr [have_size], 1
    jmp .skip
.try_auto:
    cmp al, 'A'
    jne .try_create
    cmp byte ptr [si + 1], 'U'
    jne .try_create
    cmp byte ptr [si + 2], 'T'
    jne .try_create
    cmp byte ptr [si + 3], 'O'
    jne .try_create
    mov byte ptr [want_create], 1
    add si, 4
    jmp .skip
.try_create:
    cmp al, 'C'
    jne .bad
    cmp byte ptr [si + 1], 'R'
    jne .bad
    cmp byte ptr [si + 2], 'E'
    jne .bad
    cmp byte ptr [si + 3], 'A'
    jne .bad
    cmp byte ptr [si + 4], 'T'
    jne .bad
    cmp byte ptr [si + 5], 'E'
    jne .bad
    /* CREATEEXT / CREATELOG / CREATE */
    cmp byte ptr [si + 6], 'E'
    jne .try_clog
    cmp byte ptr [si + 7], 'X'
    jne .try_clog
    cmp byte ptr [si + 8], 'T'
    jne .try_clog
    mov byte ptr [want_create_ext], 1
    add si, 9
    jmp .skip
.try_clog:
    cmp byte ptr [si + 6], 'L'
    jne .try_cpri
    cmp byte ptr [si + 7], 'O'
    jne .try_cpri
    cmp byte ptr [si + 8], 'G'
    jne .try_cpri
    mov byte ptr [want_create_log], 1
    add si, 9
    jmp .skip
.try_cpri:
    mov byte ptr [want_create], 1
    add si, 6
    jmp .skip
.ok:
    clc
    ret
.bad:
    stc
    ret

skip_sp:
.ssp:
    mov al, [si]
    cmp al, ' '
    je .ssp_i
    cmp al, 9
    je .ssp_i
    ret
.ssp_i:
    inc si
    jmp .ssp

/* Parse decimal at SI → DX:AX (32-bit). Advances SI. CF on error. */
parse_dec:
    xor bx, bx                   /* lo */
    xor di, di                   /* hi */
    mov word ptr [parse_cnt], 0
.pd:
    mov al, [si]
    cmp al, '0'
    jb .pd_done
    cmp al, '9'
    ja .pd_done
    sub al, '0'
    xor ah, ah
    push ax                      /* digit */
    mov ax, bx
    mov cx, 10
    mul cx                       /* DX:AX = lo*10 */
    mov bx, ax
    mov cx, dx                   /* carry into hi */
    mov ax, di
    mov dx, 10
    mul dx                       /* DX:AX = hi*10 */
    add ax, cx
    adc dx, 0
    mov di, ax
    pop ax
    add bx, ax
    adc di, 0
    inc si
    inc word ptr [parse_cnt]
    jmp .pd
.pd_done:
    cmp word ptr [parse_cnt], 0
    je .pd_bad
    mov ax, bx
    mov dx, di
    clc
    ret
.pd_bad:
    stc
    ret

geometry:
    push si
    mov ah, 0x08
    mov dl, [drive]
    int 0x13
    jc .geo_bad
    mov al, cl
    and ax, 0x003F
    jz .geo_bad
    mov [spt], ax
    mov al, dh
    xor ah, ah
    inc ax
    mov [heads], ax
    mov al, ch
    mov ah, cl
    mov cl, 6
    shr ah, cl
    inc ax
    mul word ptr [heads]         /* DX:AX = cylinders * heads */
    mov bx, ax
    mov cx, dx
    mov ax, bx
    mul word ptr [spt]           /* DX:AX = lo * spt */
    mov bx, ax
    mov si, dx
    mov ax, cx
    mul word ptr [spt]           /* AX = hi * spt */
    add ax, si
    adc dx, 0
    mov dx, ax                   /* DX:AX = total sectors */
    mov ax, bx
    mov [total], ax
    mov [total + 2], dx
    cmp dx, 0
    jne .geo_ok
    cmp ax, 18
    jbe .geo_bad
.geo_ok:
    pop si
    clc
    ret
.geo_bad:
    pop si
    stc
    ret

/* DX:AX=LBA, ES:BX=buffer. CF from INT 13h. DX:AX and BX preserved. */
read_lba:
    push ax
    push bx
    push cx
    push dx
    push di
    push si
    mov di, bx
    mov cx, [spt]
    div cx                       /* DX:AX / spt → AX=temp, DX=sec rem */
    mov si, dx
    inc si
    xor dx, dx
    mov bx, [heads]
    div bx                       /* AX=cyl, DX=head */
    mov dh, dl
    mov ch, al
    mov cl, 6
    shl ah, cl
    mov bx, si
    or ah, bl
    mov cl, ah
    mov dl, [drive]
    mov bx, di
    mov ax, 0x0201
    int 0x13
    pop si
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

write_lba:
    push ax
    push bx
    push cx
    push dx
    push di
    push si
    mov di, bx
    mov cx, [spt]
    div cx
    mov si, dx
    inc si
    xor dx, dx
    mov bx, [heads]
    div bx
    mov dh, dl
    mov ch, al
    mov cl, 6
    shl ah, cl
    mov bx, si
    or ah, bl
    mov cl, ah
    mov dl, [drive]
    mov bx, di
    mov ax, 0x0301
    int 0x13
    pop si
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

clear_buf:
    push ax
    push cx
    push di
    lea di, [secbuf]
    xor ax, ax
    mov cx, 256
    rep stosw
    pop di
    pop cx
    pop ax
    ret

install_mbr_code:
    push ds
    push es
    push cs
    pop ds
    lea si, [mbr_boot]
    lea di, [secbuf]
    mov cx, 223
    rep movsw
    pop es
    pop ds
    ret

/* Fill partition entry at ES:DI from part_start/part_secs. AH=0 → active. */
fill_entry:
    mov byte ptr [di], 0
    test ah, ah
    jnz .fe_chs
    mov byte ptr [di], 0x80
.fe_chs:
    mov ax, [part_start]
    mov dx, [part_start + 2]
    push di
    mov cx, [spt]
    div cx
    mov bl, dl
    inc bl
    xor dx, dx
    mov cx, [heads]
    div cx
    pop di
    mov [di + 1], dl
    mov [di + 3], al
    mov al, ah
    and al, 3
    mov cl, 6
    shl al, cl
    or al, bl
    mov [di + 2], al
    /* < 16MB → 01h; < 32MB → 04h; else → 06h */
    mov ax, [part_secs]
    mov dx, [part_secs + 2]
    test dx, dx
    jnz .fe_t6
    cmp ax, 32768
    jae .fe_t4
    mov byte ptr [di + 4], 0x01
    jmp .fe_tend
.fe_t4:
    mov byte ptr [di + 4], 0x04
    jmp .fe_tend
.fe_t6:
    mov byte ptr [di + 4], 0x06
.fe_tend:
    mov byte ptr [di + 5], 0xFF
    mov byte ptr [di + 6], 0xFF
    mov byte ptr [di + 7], 0xFF
    mov ax, [part_start]
    mov [di + 8], ax
    mov ax, [part_start + 2]
    mov [di + 10], ax
    mov ax, [part_secs]
    mov [di + 12], ax
    mov ax, [part_secs + 2]
    mov [di + 14], ax
    ret

do_create:
    call geometry
    jc .dc_bad
    /* load or init MBR */
    call clear_buf
    xor ax, ax
    xor dx, dx
    lea bx, [secbuf]
    call read_lba
    jc .dc_fresh
    cmp word ptr [secbuf + 510], 0xAA55
    je .dc_have
.dc_fresh:
    call clear_buf
    call install_mbr_code
    mov word ptr [secbuf + 510], 0xAA55
.dc_have:
    /* find free slot; track next start LBA */
    mov word ptr [part_start], 17
    mov word ptr [part_start + 2], 0
    mov byte ptr [slot], 0
    mov si, 0x1BE
    mov cx, 4
.dc_scan:
    mov al, [secbuf + si + 4]
    test al, al
    jz .dc_free
    /* occupied: next start = end of this part if greater */
    mov ax, [secbuf + si + 8]
    mov dx, [secbuf + si + 10]
    add ax, [secbuf + si + 12]
    adc dx, [secbuf + si + 14]
    cmp dx, [part_start + 2]
    ja .dc_adv
    jb .dc_next
    cmp ax, [part_start]
    jbe .dc_next
.dc_adv:
    mov [part_start], ax
    mov [part_start + 2], dx
.dc_next:
    add si, 16
    inc byte ptr [slot]
    loop .dc_scan
    jmp .dc_bad                    /* no free slot */
.dc_free:
    /* SI = free entry offset; part_start set */
    mov ax, [total]
    mov dx, [total + 2]
    sub ax, [part_start]
    sbb dx, [part_start + 2]
    jc .dc_bad
    jnz .dc_have_rest
    test ax, ax
    jz .dc_bad
.dc_have_rest:
    cmp byte ptr [have_size], 0
    je .dc_use_rest
    cmp word ptr [req_size], 0
    jne .dc_chk_fit
    cmp word ptr [req_size + 2], 0
    je .dc_bad
.dc_chk_fit:
    mov bx, [req_size + 2]
    cmp bx, dx
    ja .dc_bad
    jb .dc_use_req
    mov bx, [req_size]
    cmp bx, ax
    ja .dc_bad
.dc_use_req:
    mov ax, [req_size]
    mov dx, [req_size + 2]
.dc_use_rest:
    mov [part_secs], ax
    mov [part_secs + 2], dx
    /* first primary active only */
    xor ah, ah
    cmp byte ptr [slot], 0
    je .dc_fill
    mov ah, 1
.dc_fill:
    push si
    lea di, [secbuf]
    add di, si
    call fill_entry
    pop si
    mov word ptr [secbuf + 510], 0xAA55
    xor ax, ax
    xor dx, dx
    lea bx, [secbuf]
    call write_lba
    jc .dc_bad
    /* VBR template at part_start */
    call clear_buf
    push ds
    push es
    push cs
    pop ds
    lea si, [vbr_boot]
    lea di, [secbuf]
    mov cx, 256
    rep movsw
    pop es
    pop ds
    mov word ptr [secbuf + 510], 0xAA55
    mov ax, [part_start]
    mov dx, [part_start + 2]
    lea bx, [secbuf]
    call write_lba
    jc .dc_bad
    clc
    ret
.dc_bad:
    stc
    ret

/*
 * Create type-05 extended container in next free primary slot.
 * Leaves first EBR empty (no logical yet) with AA55 signature.
 */
do_create_ext:
    call geometry
    jc .dce_bad
    call clear_buf
    xor ax, ax
    xor dx, dx
    lea bx, [secbuf]
    call read_lba
    jc .dce_fresh
    cmp word ptr [secbuf + 510], 0xAA55
    je .dce_have
.dce_fresh:
    call clear_buf
    call install_mbr_code
    mov word ptr [secbuf + 510], 0xAA55
.dce_have:
    mov word ptr [part_start], 17
    mov word ptr [part_start + 2], 0
    mov byte ptr [slot], 0
    mov si, 0x1BE
    mov cx, 4
.dce_scan:
    mov al, [secbuf + si + 4]
    test al, al
    jz .dce_free
    mov ax, [secbuf + si + 8]
    mov dx, [secbuf + si + 10]
    add ax, [secbuf + si + 12]
    adc dx, [secbuf + si + 14]
    cmp dx, [part_start + 2]
    ja .dce_adv
    jb .dce_next
    cmp ax, [part_start]
    jbe .dce_next
.dce_adv:
    mov [part_start], ax
    mov [part_start + 2], dx
.dce_next:
    add si, 16
    inc byte ptr [slot]
    loop .dce_scan
    jmp .dce_bad
.dce_free:
    mov ax, [total]
    mov dx, [total + 2]
    sub ax, [part_start]
    sbb dx, [part_start + 2]
    jc .dce_bad
    jnz .dce_have_rest
    test ax, ax
    jz .dce_bad
.dce_have_rest:
    cmp byte ptr [have_size], 0
    je .dce_use_rest
    cmp word ptr [req_size], 0
    jne .dce_chk_fit
    cmp word ptr [req_size + 2], 0
    je .dce_bad
.dce_chk_fit:
    mov bx, [req_size + 2]
    cmp bx, dx
    ja .dce_bad
    jb .dce_use_req
    mov bx, [req_size]
    cmp bx, ax
    ja .dce_bad
.dce_use_req:
    mov ax, [req_size]
    mov dx, [req_size + 2]
.dce_use_rest:
    mov [part_secs], ax
    mov [part_secs + 2], dx
    push si
    lea di, [secbuf]
    add di, si
    call fill_entry_ext
    pop si
    mov word ptr [secbuf + 510], 0xAA55
    xor ax, ax
    xor dx, dx
    lea bx, [secbuf]
    call write_lba
    jc .dce_bad
    /* Empty first EBR at part_start */
    call clear_buf
    mov word ptr [secbuf + 510], 0xAA55
    mov ax, [part_start]
    mov dx, [part_start + 2]
    lea bx, [secbuf]
    call write_lba
    jc .dce_bad
    clc
    ret
.dce_bad:
    stc
    ret

/* Fill DI with type-05 extended entry from part_start/part_secs. Never active. */
fill_entry_ext:
    mov byte ptr [di], 0
    mov ax, [part_start]
    mov dx, [part_start + 2]
    push di
    mov cx, [spt]
    div cx
    mov bl, dl
    inc bl
    xor dx, dx
    mov cx, [heads]
    div cx
    pop di
    mov [di + 1], dl
    mov [di + 3], al
    mov al, ah
    and al, 3
    mov cl, 6
    shl al, cl
    or al, bl
    mov [di + 2], al
    mov byte ptr [di + 4], 0x05
    mov byte ptr [di + 5], 0xFF
    mov byte ptr [di + 6], 0xFF
    mov byte ptr [di + 7], 0xFF
    mov ax, [part_start]
    mov [di + 8], ax
    mov ax, [part_start + 2]
    mov [di + 10], ax
    mov ax, [part_secs]
    mov [di + 12], ax
    mov ax, [part_secs + 2]
    mov [di + 14], ax
    ret

/*
 * Add a logical DOS volume inside the first extended partition.
 */
do_create_log:
    call geometry
    jc .dcl_bad
    call clear_buf
    xor ax, ax
    xor dx, dx
    lea bx, [secbuf]
    call read_lba
    jc .dcl_bad
    cmp word ptr [secbuf + 510], 0xAA55
    jne .dcl_bad
    /* Find first extended */
    mov si, 0x1BE
    mov cx, 4
.dcl_find_ext:
    mov al, [secbuf + si + 4]
    cmp al, 0x05
    je .dcl_got_ext
    cmp al, 0x0F
    je .dcl_got_ext
    add si, 16
    loop .dcl_find_ext
    jmp .dcl_bad
.dcl_got_ext:
    mov ax, [secbuf + si + 8]
    mov [ext_base], ax
    mov ax, [secbuf + si + 10]
    mov [ext_base + 2], ax
    mov ax, [secbuf + si + 12]
    mov [ext_secs], ax
    mov ax, [secbuf + si + 14]
    mov [ext_secs + 2], ax
    /* Walk EBR chain; find empty first entry or append */
    mov ax, [ext_base]
    mov [ebr_lba], ax
    mov ax, [ext_base + 2]
    mov [ebr_lba + 2], ax
    mov word ptr [prev_ebr], 0xFFFF
    mov word ptr [prev_ebr + 2], 0xFFFF
.dcl_walk:
    mov ax, [ebr_lba]
    mov dx, [ebr_lba + 2]
    lea bx, [secbuf]
    call read_lba
    jc .dcl_bad
    cmp word ptr [secbuf + 510], 0xAA55
    jne .dcl_bad
    mov al, [secbuf + 0x1BE + 4]
    test al, al
    jz .dcl_fill_here
    /* occupied: follow link or append after this logical */
    mov al, [secbuf + 0x1CE + 4]
    cmp al, 0x05
    je .dcl_follow
    cmp al, 0x0F
    je .dcl_follow
    /* No next link: create new EBR after this logical */
    mov ax, [ebr_lba]
    mov dx, [ebr_lba + 2]
    add ax, [secbuf + 0x1BE + 8]
    adc dx, [secbuf + 0x1BE + 10]
    add ax, [secbuf + 0x1BE + 12]
    adc dx, [secbuf + 0x1BE + 14]
    jc .dcl_bad
    mov [new_ebr], ax
    mov [new_ebr + 2], dx
    mov ax, [new_ebr]
    mov dx, [new_ebr + 2]
    sub ax, [ext_base]
    sbb dx, [ext_base + 2]
    mov [secbuf + 0x1CE + 8], ax
    mov [secbuf + 0x1CE + 10], dx
    mov byte ptr [secbuf + 0x1CE + 4], 0x05
    mov byte ptr [secbuf + 0x1CE], 0
    mov byte ptr [secbuf + 0x1CE + 5], 0xFF
    mov byte ptr [secbuf + 0x1CE + 6], 0xFF
    mov byte ptr [secbuf + 0x1CE + 7], 0xFF
    mov ax, [ext_base]
    mov dx, [ext_base + 2]
    add ax, [ext_secs]
    adc dx, [ext_secs + 2]
    sub ax, [new_ebr]
    sbb dx, [new_ebr + 2]
    mov [secbuf + 0x1CE + 12], ax
    mov [secbuf + 0x1CE + 14], dx
    mov ax, [ebr_lba]
    mov dx, [ebr_lba + 2]
    lea bx, [secbuf]
    call write_lba
    jc .dcl_bad
    mov ax, [new_ebr]
    mov [ebr_lba], ax
    mov ax, [new_ebr + 2]
    mov [ebr_lba + 2], ax
    call clear_buf
    mov word ptr [secbuf + 510], 0xAA55
    jmp .dcl_fill_here
.dcl_follow:
    mov ax, [ext_base]
    mov dx, [ext_base + 2]
    add ax, [secbuf + 0x1CE + 8]
    adc dx, [secbuf + 0x1CE + 10]
    mov [prev_ebr], ax
    mov [prev_ebr + 2], dx
    mov [ebr_lba], ax
    mov [ebr_lba + 2], dx
    jmp .dcl_walk

.dcl_fill_here:
    mov word ptr [part_start], 1
    mov word ptr [part_start + 2], 0
    mov ax, [ext_base]
    mov dx, [ext_base + 2]
    add ax, [ext_secs]
    adc dx, [ext_secs + 2]
    sub ax, [ebr_lba]
    sbb dx, [ebr_lba + 2]
    sub ax, 1
    sbb dx, 0
    jc .dcl_bad
    jnz .dcl_have_rest
    test ax, ax
    jz .dcl_bad
.dcl_have_rest:
    cmp byte ptr [have_size], 0
    je .dcl_use_rest
    cmp word ptr [req_size], 0
    jne .dcl_chk_fit
    cmp word ptr [req_size + 2], 0
    je .dcl_bad
.dcl_chk_fit:
    mov bx, [req_size + 2]
    cmp bx, dx
    ja .dcl_bad
    jb .dcl_use_req
    mov bx, [req_size]
    cmp bx, ax
    ja .dcl_bad
.dcl_use_req:
    mov ax, [req_size]
    mov dx, [req_size + 2]
.dcl_use_rest:
    mov [part_secs], ax
    mov [part_secs + 2], dx
    lea di, [secbuf + 0x1BE]
    mov byte ptr [di], 0
    mov byte ptr [di + 1], 0
    mov byte ptr [di + 2], 1
    mov byte ptr [di + 3], 0
    mov ax, [part_secs]
    mov dx, [part_secs + 2]
    test dx, dx
    jnz .dcl_t6
    cmp ax, 32768
    jae .dcl_t4
    mov byte ptr [di + 4], 0x01
    jmp .dcl_tend
.dcl_t4:
    mov byte ptr [di + 4], 0x04
    jmp .dcl_tend
.dcl_t6:
    mov byte ptr [di + 4], 0x06
.dcl_tend:
    mov byte ptr [di + 5], 0xFF
    mov byte ptr [di + 6], 0xFF
    mov byte ptr [di + 7], 0xFF
    mov word ptr [di + 8], 1
    mov word ptr [di + 10], 0
    mov ax, [part_secs]
    mov [di + 12], ax
    mov ax, [part_secs + 2]
    mov [di + 14], ax
    mov byte ptr [secbuf + 0x1CE + 4], 0
    mov word ptr [secbuf + 510], 0xAA55
    mov ax, [ebr_lba]
    mov dx, [ebr_lba + 2]
    lea bx, [secbuf]
    call write_lba
    jc .dcl_bad
    call clear_buf
    push ds
    push es
    push cs
    pop ds
    lea si, [vbr_boot]
    lea di, [secbuf]
    mov cx, 256
    rep movsw
    pop es
    pop ds
    mov word ptr [secbuf + 510], 0xAA55
    mov ax, [ebr_lba]
    mov dx, [ebr_lba + 2]
    add ax, 1
    adc dx, 0
    lea bx, [secbuf]
    call write_lba
    jc .dcl_bad
    clc
    ret
.dcl_bad:
    stc
    ret

do_list:
    call geometry
    jc .dl_bad
    lea dx, [msg_hd]
    mov ah, 9
    int 0x21
    call clear_buf
    xor ax, ax
    xor dx, dx
    lea bx, [secbuf]
    call read_lba
    jc .dl_none
    cmp word ptr [secbuf + 510], 0xAA55
    jne .dl_none
    mov byte ptr [list_let], 'C'
    mov word ptr [list_ext0], 0
    mov word ptr [list_ext0 + 2], 0
    mov word ptr [list_ext1], 0
    mov word ptr [list_ext1 + 2], 0
    mov si, 0x1BE
    mov cx, 4
    xor di, di
.dl_scan:
    mov al, [secbuf + si + 4]
    cmp al, 0x01
    je .dl_dos
    cmp al, 0x04
    je .dl_dos
    cmp al, 0x06
    je .dl_dos
    cmp al, 0x05
    je .dl_ext
    cmp al, 0x0F
    je .dl_ext
    jmp .dl_n
.dl_dos:
    mov ax, [secbuf + si + 8]
    or ax, [secbuf + si + 10]
    jz .dl_n
    mov al, [list_let]
    mov [msg_let], al
    lea dx, [msg_let_line]
    mov ah, 9
    int 0x21
    inc byte ptr [list_let]
    jmp .dl_n
.dl_ext:
    mov ax, [secbuf + si + 8]
    or ax, [secbuf + si + 10]
    jz .dl_n
    cmp di, 8
    jae .dl_n
    mov ax, [secbuf + si + 8]
    mov [list_ext0 + di], ax
    mov ax, [secbuf + si + 10]
    mov [list_ext0 + di + 2], ax
    add di, 4
.dl_n:
    add si, 16
    loop .dl_scan
    /* Walk collected extended bases after MBR scan */
    xor di, di
    mov cx, 2
.dl_walk_exts:
    mov ax, [list_ext0 + di]
    or ax, [list_ext0 + di + 2]
    jz .dl_we_next
    mov ax, [list_ext0 + di]
    mov [ext_base], ax
    mov ax, [list_ext0 + di + 2]
    mov [ext_base + 2], ax
    call list_logicals
.dl_we_next:
    add di, 4
    loop .dl_walk_exts
.dl_none:
    clc
    ret
.dl_bad:
    stc
    ret

/* List logicals in extended at ext_base; advances list_let. */
list_logicals:
    push ax
    push bx
    push cx
    push dx
    mov ax, [ext_base]
    mov [ebr_lba], ax
    mov ax, [ext_base + 2]
    mov [ebr_lba + 2], ax
.ll_loop:
    mov ax, [ebr_lba]
    mov dx, [ebr_lba + 2]
    lea bx, [secbuf]
    call read_lba
    jc .ll_done
    cmp word ptr [secbuf + 510], 0xAA55
    jne .ll_done
    mov al, [secbuf + 0x1BE + 4]
    cmp al, 0x01
    je .ll_dos
    cmp al, 0x04
    je .ll_dos
    cmp al, 0x06
    jne .ll_link
.ll_dos:
    mov al, [list_let]
    mov [msg_let], al
    lea dx, [msg_let_line]
    mov ah, 9
    int 0x21
    inc byte ptr [list_let]
.ll_link:
    mov al, [secbuf + 0x1CE + 4]
    cmp al, 0x05
    je .ll_next
    cmp al, 0x0F
    jne .ll_done
.ll_next:
    mov ax, [ext_base]
    mov dx, [ext_base + 2]
    add ax, [secbuf + 0x1CE + 8]
    adc dx, [secbuf + 0x1CE + 10]
    cmp dx, [ebr_lba + 2]
    jne .ll_go
    cmp ax, [ebr_lba]
    je .ll_done
.ll_go:
    mov [ebr_lba], ax
    mov [ebr_lba + 2], dx
    jmp .ll_loop
.ll_done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* Copied verbatim to sector zero.  It loads RFAT1 at 0600 then KERNEL.SYS. */
mbr_boot:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov si, 0x7BE
    mov cx, 4
.mbr_scan:
    cmp byte ptr [si], 0x80
    jne .mbr_next
    cmp byte ptr [si + 4], 0
    je .mbr_next
    mov dh, [si + 1]
    mov cl, [si + 2]
    mov ch, [si + 3]
    mov bx, 0x7C00
    mov ax, 0x0201
    int 0x13
    jc .mbr_hang
    jmp 0x0000:0x7C00
.mbr_next:
    add si, 16
    loop .mbr_scan
.mbr_hang:
    hlt
    jmp .mbr_hang
    .space 446 - (. - mbr_boot), 0

vbr_boot:
    jmp short .vbr_start
    nop
    .space 59, 0
.vbr_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [0x7C24], dl
    mov ax, [0x7C1C]
    inc ax                        /* hidden + RFAT1 */
    mov bx, 0x0600
    call .vbr_read
    jc .vbr_hang
    cmp dword ptr [0x0600], 0x54414652
    jne .vbr_hang
    mov si, [0x061C]
    add si, [0x7C1C]
    mov di, [0x061E]
    mov ax, 0x0070
    mov es, ax
    xor bx, bx
.vbr_load:
    test di, di
    jz .vbr_go
    mov ax, si
    call .vbr_read
    jc .vbr_hang
    inc si
    add bx, 512
    dec di
    jmp .vbr_load
.vbr_go:
    mov dl, [0x7C24]
    sti
    jmp 0x0070:0
.vbr_read:
    push ax
    push bx
    push cx
    push dx
    push si
    mov si, bx
    xor dx, dx
    div word ptr [0x7C18]
    mov cl, dl
    inc cl
    xor dx, dx
    div word ptr [0x7C1A]
    mov dh, dl
    mov ch, al
    mov al, ah
    mov ah, cl
    mov cl, 6
    shl al, cl
    or al, ah
    mov cl, al
    mov dl, [0x7C24]
    mov bx, si
    mov ax, 0x0201
    int 0x13
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret
.vbr_hang:
    hlt
    jmp .vbr_hang
    .space 512 - (. - vbr_boot), 0

drive:      .byte 0x80
list_only:  .byte 0
want_create:.byte 0
want_create_ext:.byte 0
want_create_log:.byte 0
have_size:  .byte 0
slot:       .byte 0
list_let:   .byte 'C'
req_size:   .word 0, 0
spt:        .word 17
heads:      .word 4
total:      .word 0, 0
part_start: .word 0, 0
part_secs:  .word 0, 0
ext_base:   .word 0, 0
ext_secs:   .word 0, 0
ebr_lba:    .word 0, 0
prev_ebr:   .word 0, 0
new_ebr:    .word 0, 0
list_ext0:  .word 0, 0
list_ext1:  .word 0, 0
parse_cnt:  .word 0
secbuf:     .space 512, 0
msg_ok:     .ascii "PARTEDIT OK\r\n$"
msg_hd:     .ascii "HD 80\r\n$"
msg_let_line:
msg_let:    .ascii "C:\r\n$"
msg_usage:  .ascii "PARTEDIT [/CREATE|/CREATEEXT|/CREATELOG [/SIZE n]|/LIST] [C:]\r\n$"
msg_fail:   .ascii "PARTEDIT failed\r\n$"
