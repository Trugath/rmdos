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
 * sector count; omit to use remaining disk space after track 0.
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

/* Parse decimal at SI → AX. Advances SI. CF on error. */
parse_dec:
    xor bx, bx
    mov cx, 0
.pd:
    mov al, [si]
    cmp al, '0'
    jb .pd_done
    cmp al, '9'
    ja .pd_done
    sub al, '0'
    xor ah, ah
    push ax
    mov ax, bx
    mov dx, 10
    mul dx
    mov bx, ax
    pop ax
    add bx, ax
    inc si
    inc cx
    jmp .pd
.pd_done:
    test cx, cx
    jz .pd_bad
    mov ax, bx
    clc
    ret
.pd_bad:
    stc
    ret

geometry:
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
    mul word ptr [heads]
    mul word ptr [spt]
    test dx, dx
    jnz .geo_bad
    cmp ax, 18
    jbe .geo_bad
    mov [total], ax
    clc
    ret
.geo_bad:
    stc
    ret

/* AX=LBA, ES:BX=buffer. CF from INT 13h. */
read_lba:
    push ax
    push bx
    push cx
    push dx
    push si
    mov si, bx
    xor dx, dx
    div word ptr [spt]
    mov cl, dl
    inc cl
    xor dx, dx
    div word ptr [heads]
    mov dh, dl
    mov ch, al
    mov al, ah
    mov ah, cl
    mov cl, 6
    shl al, cl
    or al, ah
    mov cl, al
    mov dl, [drive]
    mov bx, si
    mov ax, 0x0201
    int 0x13
    pop si
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
    push si
    mov si, bx
    xor dx, dx
    div word ptr [spt]
    mov cl, dl
    inc cl
    xor dx, dx
    div word ptr [heads]
    mov dh, dl
    mov ch, al
    mov al, ah
    mov ah, cl
    mov cl, 6
    shl al, cl
    or al, ah
    mov cl, al
    mov dl, [drive]
    mov bx, si
    mov ax, 0x0301
    int 0x13
    pop si
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
    xor dx, dx
    div word ptr [spt]
    mov bl, dl
    inc bl
    xor dx, dx
    div word ptr [heads]
    mov [di + 1], dl
    mov [di + 3], al
    mov al, ah
    and al, 3
    mov cl, 6
    shl al, cl
    or al, bl
    mov [di + 2], al
    mov ax, [part_secs]
    /* < 16MB → 01h; < 32MB → 04h; else → 06h */
    cmp ax, 32768
    jae .fe_t4
    mov byte ptr [di + 4], 0x01
    jmp .fe_tend
.fe_t4:
    cmp ax, 65535
    ja .fe_t6
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
    mov word ptr [di + 10], 0
    mov ax, [part_secs]
    mov [di + 12], ax
    mov word ptr [di + 14], 0
    ret

do_create:
    call geometry
    jc .dc_bad
    /* load or init MBR */
    call clear_buf
    xor ax, ax
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
    mov byte ptr [slot], 0
    mov si, 0x1BE
    mov cx, 4
.dc_scan:
    mov al, [secbuf + si + 4]
    test al, al
    jz .dc_free
    /* occupied: next start = end of this part if greater */
    mov ax, [secbuf + si + 8]
    add ax, [secbuf + si + 12]
    cmp ax, [part_start]
    jbe .dc_next
    mov [part_start], ax
.dc_next:
    add si, 16
    inc byte ptr [slot]
    loop .dc_scan
    jmp .dc_bad                    /* no free slot */
.dc_free:
    /* SI = free entry offset; part_start set */
    mov ax, [total]
    sub ax, [part_start]
    jbe .dc_bad
    cmp byte ptr [have_size], 0
    je .dc_use_rest
    mov bx, [req_size]
    test bx, bx
    jz .dc_bad
    cmp bx, ax
    ja .dc_bad
    mov ax, bx
.dc_use_rest:
    mov [part_secs], ax
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
    mov byte ptr [slot], 0
    mov si, 0x1BE
    mov cx, 4
.dce_scan:
    mov al, [secbuf + si + 4]
    test al, al
    jz .dce_free
    mov ax, [secbuf + si + 8]
    add ax, [secbuf + si + 12]
    cmp ax, [part_start]
    jbe .dce_next
    mov [part_start], ax
.dce_next:
    add si, 16
    inc byte ptr [slot]
    loop .dce_scan
    jmp .dce_bad
.dce_free:
    mov ax, [total]
    sub ax, [part_start]
    jbe .dce_bad
    cmp byte ptr [have_size], 0
    je .dce_use_rest
    mov bx, [req_size]
    test bx, bx
    jz .dce_bad
    cmp bx, ax
    ja .dce_bad
    mov ax, bx
.dce_use_rest:
    mov [part_secs], ax
    push si
    lea di, [secbuf]
    add di, si
    call fill_entry_ext
    pop si
    mov word ptr [secbuf + 510], 0xAA55
    xor ax, ax
    lea bx, [secbuf]
    call write_lba
    jc .dce_bad
    /* Empty first EBR at part_start */
    call clear_buf
    mov word ptr [secbuf + 510], 0xAA55
    mov ax, [part_start]
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
    xor dx, dx
    div word ptr [spt]
    mov bl, dl
    inc bl
    xor dx, dx
    div word ptr [heads]
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
    mov word ptr [di + 10], 0
    mov ax, [part_secs]
    mov [di + 12], ax
    mov word ptr [di + 14], 0
    ret

/*
 * Add a logical DOS volume inside the first extended partition.
 */
do_create_log:
    call geometry
    jc .dcl_bad
    call clear_buf
    xor ax, ax
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
    mov ax, [secbuf + si + 12]
    mov [ext_secs], ax
    /* Walk EBR chain; find empty first entry or append */
    mov ax, [ext_base]
    mov [ebr_lba], ax
    mov word ptr [prev_ebr], 0xFFFF
.dcl_walk:
    mov ax, [ebr_lba]
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
    add ax, [secbuf + 0x1BE + 8]
    add ax, [secbuf + 0x1BE + 12]
    jc .dcl_bad
    mov [new_ebr], ax
    /* Link from current EBR */
    mov ax, [new_ebr]
    sub ax, [ext_base]
    mov [secbuf + 0x1CE + 8], ax
    mov word ptr [secbuf + 0x1CE + 10], 0
    /* size of link entry = remaining; CHS junk FF */
    mov byte ptr [secbuf + 0x1CE + 4], 0x05
    mov byte ptr [secbuf + 0x1CE], 0
    mov byte ptr [secbuf + 0x1CE + 5], 0xFF
    mov byte ptr [secbuf + 0x1CE + 6], 0xFF
    mov byte ptr [secbuf + 0x1CE + 7], 0xFF
    mov ax, [ext_base]
    add ax, [ext_secs]
    sub ax, [new_ebr]
    mov [secbuf + 0x1CE + 12], ax
    mov word ptr [secbuf + 0x1CE + 14], 0
    mov ax, [ebr_lba]
    lea bx, [secbuf]
    call write_lba
    jc .dcl_bad
    mov ax, [new_ebr]
    mov [ebr_lba], ax
    call clear_buf
    mov word ptr [secbuf + 510], 0xAA55
    jmp .dcl_fill_here
.dcl_follow:
    mov ax, [ext_base]
    add ax, [secbuf + 0x1CE + 8]
    mov [prev_ebr], ax
    mov word ptr [ebr_lba], ax
    jmp .dcl_walk

.dcl_fill_here:
    /* First sector of EBR is reserved; logical starts at ebr+1 */
    mov word ptr [part_start], 1
    mov ax, [ext_base]
    add ax, [ext_secs]
    sub ax, [ebr_lba]
    dec ax                       /* minus EBR sector */
    jbe .dcl_bad
    cmp byte ptr [have_size], 0
    je .dcl_use_rest
    mov bx, [req_size]
    test bx, bx
    jz .dcl_bad
    cmp bx, ax
    ja .dcl_bad
    mov ax, bx
.dcl_use_rest:
    mov [part_secs], ax
    /* Write logical entry at 0x1BE relative to this EBR */
    lea di, [secbuf + 0x1BE]
    mov byte ptr [di], 0
    /* crude CHS */
    mov byte ptr [di + 1], 0
    mov byte ptr [di + 2], 1
    mov byte ptr [di + 3], 0
    mov ax, [part_secs]
    cmp ax, 32768
    jae .dcl_t4
    mov byte ptr [di + 4], 0x01
    jmp .dcl_tend
.dcl_t4:
    cmp ax, 65535
    ja .dcl_t6
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
    mov word ptr [di + 14], 0
    /* clear link slot if fresh EBR */
    mov byte ptr [secbuf + 0x1CE + 4], 0
    mov word ptr [secbuf + 510], 0xAA55
    mov ax, [ebr_lba]
    lea bx, [secbuf]
    call write_lba
    jc .dcl_bad
    /* VBR at ebr_lba+1 */
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
    inc ax
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
    lea bx, [secbuf]
    call read_lba
    jc .dl_none
    cmp word ptr [secbuf + 510], 0xAA55
    jne .dl_none
    mov byte ptr [list_let], 'C'
    mov word ptr [list_ext0], 0
    mov word ptr [list_ext1], 0
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
    cmp word ptr [secbuf + si + 10], 0
    jne .dl_n
    cmp word ptr [secbuf + si + 8], 0
    je .dl_n
    mov al, [list_let]
    mov [msg_let], al
    lea dx, [msg_let_line]
    mov ah, 9
    int 0x21
    inc byte ptr [list_let]
    jmp .dl_n
.dl_ext:
    cmp word ptr [secbuf + si + 10], 0
    jne .dl_n
    mov ax, [secbuf + si + 8]
    test ax, ax
    jz .dl_n
    cmp di, 4
    jae .dl_n
    mov [list_ext0 + di], ax
    add di, 2
.dl_n:
    add si, 16
    loop .dl_scan
    /* Walk collected extended bases after MBR scan */
    xor di, di
    mov cx, 2
.dl_walk_exts:
    mov ax, [list_ext0 + di]
    test ax, ax
    jz .dl_we_next
    mov [ext_base], ax
    call list_logicals
.dl_we_next:
    add di, 2
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
.ll_loop:
    mov ax, [ebr_lba]
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
    add ax, [secbuf + 0x1CE + 8]
    cmp ax, [ebr_lba]
    je .ll_done
    mov [ebr_lba], ax
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
req_size:   .word 0
spt:        .word 17
heads:      .word 4
total:      .word 0
part_start: .word 0
part_secs:  .word 0
ext_base:   .word 0
ext_secs:   .word 0
ebr_lba:    .word 0
prev_ebr:   .word 0
new_ebr:    .word 0
list_ext0:  .word 0
list_ext1:  .word 0
secbuf:     .space 512, 0
msg_ok:     .ascii "PARTEDIT OK\r\n$"
msg_hd:     .ascii "HD 80\r\n$"
msg_let_line:
msg_let:    .ascii "C:\r\n$"
msg_usage:  .ascii "PARTEDIT [/CREATE|/CREATEEXT|/CREATELOG [/SIZE n]|/LIST] [C:]\r\n$"
msg_fail:   .ascii "PARTEDIT failed\r\n$"
