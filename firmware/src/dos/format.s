.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * FORMAT.COM — FAT12/FAT16 format from INT 13h geometry (floppy or HDD ≤128MB).
 * Usage: FORMAT [d:] [/S] [/Y] [/V[:label]] [/F:720] [/1] [/4]
 */

.equ ROOT_ENTS, 112
.equ RESERVED, 2

_start:
    push cs
    pop ds
    push cs
    pop es
    mov byte ptr [flag_y], 0
    mov byte ptr [flag_s], 0
    mov byte ptr [flag_v], 0
    mov byte ptr [flag_f720], 0
    mov byte ptr [flag_one], 0
    mov byte ptr [flag_four], 0
    mov byte ptr [preset_media], 0
    mov byte ptr [drive_dl], 0
    mov byte ptr [drive_let], 'A'
    mov byte ptr [drive_idx], 0
    mov word ptr [kern_seg], 0
    mov word ptr [cmd_seg], 0
    mov word ptr [fat_seg], 0
    lea di, [volume_label]
    mov cx, 11
    mov al, ' '
    rep stosb
    mov si, 0x81
    call parse_args
    jc do_usage
    call get_geometry
    jc do_err_geo
    call apply_presets
    jc do_err_geo
    call compute_layout
    jc do_err_geo
    cmp byte ptr [flag_y], 0
    jne after_ask
    call print_warn
ask_loop:
    mov ah, 0x08
    int 0x21
    call up_al
    cmp al, 'Y'
    je ask_yes
    cmp al, 'N'
    je do_abort
    jmp ask_loop
ask_yes:
    mov ah, 0x09
    lea dx, [msg_crlf]
    int 0x21
after_ask:
    call prompt_volume_label
    cmp byte ptr [flag_s], 0
    je do_fmt
    mov ah, 0x09
    lea dx, [msg_load]
    int 0x21
    call load_system
    jc do_err
do_fmt:
    mov ah, 0x09
    lea dx, [msg_fmt]
    int 0x21
    call format_disk
    jc do_err
    call update_part_type
    cmp byte ptr [flag_s], 0
    je write_label
    call write_system
    jc do_err
    mov ah, 0x09
    lea dx, [msg_sys]
    int 0x21
write_label:
    call write_volume_label
    jc do_err
    /* Flush after all INT 13h layout writes so DOS remounts the new volume. */
    mov ah, 0x0D
    int 0x21
done_ok:
    call free_system
    mov ah, 0x09
    lea dx, [msg_ok]
    int 0x21
    mov ax, 0x4C00
    int 0x21
do_abort:
    mov ah, 0x09
    lea dx, [msg_crlf]
    int 0x21
    mov ah, 0x09
    lea dx, [msg_abort]
    int 0x21
    call free_system
    mov ax, 0x4C01
    int 0x21
do_err_geo:
    mov ah, 0x09
    lea dx, [msg_geo]
    int 0x21
    mov ax, 0x4C01
    int 0x21
do_err:
    call free_system
    mov ah, 0x09
    lea dx, [msg_err]
    int 0x21
    mov ax, 0x4C01
    int 0x21
do_usage:
    mov ah, 0x09
    lea dx, [msg_u]
    int 0x21
    mov ax, 0x4C01
    int 0x21

up_al:
    cmp al, 'a'
    jb up_al_done
    cmp al, 'z'
    ja up_al_done
    sub al, 0x20
up_al_done:
    ret
up_bl:
    cmp bl, 'a'
    jb up_bl_done
    cmp bl, 'z'
    ja up_bl_done
    sub bl, 0x20
up_bl_done:
    ret

clear_buf:
    push ax
    push cx
    push di
    lea di, [secbuf]
    mov cx, 256
    xor ax, ax
    rep stosw
    pop di
    pop cx
    pop ax
    ret

parse_args:
    push ax
    push bx
    push cx
    push di
pa_sp:
    mov al, [si]
    cmp al, ' '
    je pa_inc
    cmp al, 9
    je pa_inc
    jmp pa_tok
pa_inc:
    inc si
    jmp pa_sp
pa_tok:
    mov al, [si]
    test al, al
    jz pa_ok
    cmp al, 0x0D
    je pa_ok
    cmp al, '/'
    je pa_sw
    cmp al, '-'
    je pa_sw
    mov bl, al
    call up_bl
    cmp bl, 'A'
    jb pa_skip
    cmp bl, 'Z'
    ja pa_skip
    cmp byte ptr [si + 1], ':'
    jne pa_skip
    mov [drive_let], bl
    sub bl, 'A'
    mov [drive_idx], bl
    cmp bl, 2
    jb pa_setdl
    /* HD letters share BIOS 80h; get_geometry picks Nth DOS primary. */
    mov bl, 0x80
pa_setdl:
    mov [drive_dl], bl
    add si, 2
    jmp pa_sp
pa_skip:
    /* skip unknown token */
    inc si
    jmp pa_sp
pa_sw:
    inc si
    mov al, [si]
    test al, al
    jz pa_ok
    cmp al, 0x0D
    je pa_ok
    call up_al
    cmp al, 'Y'
    jne pa_try_s
    mov byte ptr [flag_y], 1
    inc si
    jmp pa_sp
pa_try_s:
    cmp al, 'S'
    jne pa_try_v
    mov byte ptr [flag_s], 1
    inc si
    jmp pa_sp
pa_try_v:
    cmp al, 'V'
    jne pa_try_f
    mov byte ptr [flag_v], 1
    inc si
    cmp byte ptr [si], ':'
    jne pa_sp
    inc si
    lea di, [volume_label]
    mov cx, 11
pa_v_copy:
    mov al, [si]
    test al, al
    jz pa_sp
    cmp al, 0x0D
    je pa_sp
    cmp al, ' '
    je pa_sp
    cmp al, 9
    je pa_sp
    test cx, cx
    jz pa_bad
    call up_al
    stosb
    inc si
    dec cx
    jmp pa_v_copy
pa_try_f:
    cmp al, 'F'
    jne pa_try_one
    cmp byte ptr [si + 1], ':'
    jne pa_bad
    cmp byte ptr [si + 2], '7'
    jne pa_bad
    cmp byte ptr [si + 3], '2'
    jne pa_bad
    cmp byte ptr [si + 4], '0'
    jne pa_bad
    mov byte ptr [flag_f720], 1
    add si, 5
    jmp pa_sp
pa_try_one:
    cmp al, '1'
    jne pa_try_four
    mov byte ptr [flag_one], 1
    inc si
    jmp pa_sp
pa_try_four:
    cmp al, '4'
    jne pa_skip
    mov byte ptr [flag_four], 1
    inc si
    jmp pa_sp
pa_ok:
    pop di
    pop cx
    pop bx
    pop ax
    clc
    ret
pa_bad:
    pop di
    pop cx
    pop bx
    pop ax
    stc
    ret

print_warn:
    mov ah, 0x09
    lea dx, [msg_warn1]
    int 0x21
    mov dl, [drive_let]
    mov ah, 0x02
    int 0x21
    mov ah, 0x09
    lea dx, [msg_warn2]
    int 0x21
    ret

/* /V without an inline label prompts for up to 11 characters. */
prompt_volume_label:
    push ax
    push cx
    push si
    push di
    push es
    cmp byte ptr [flag_v], 0
    je pvl_done
    lea si, [volume_label]
    mov cx, 11
pvl_have_label:
    cmp byte ptr [si], ' '
    jne pvl_done
    inc si
    loop pvl_have_label

    mov ah, 0x09
    lea dx, [msg_label]
    int 0x21
    mov byte ptr [label_input], 11
    mov byte ptr [label_input + 1], 0
    mov ah, 0x0A
    lea dx, [label_input]
    int 0x21

    push ds
    pop es
    lea di, [volume_label]
    mov cx, 11
    mov al, ' '
    rep stosb
    xor cx, cx
    mov cl, [label_input + 1]
    jcxz pvl_done
    lea si, [label_input + 2]
    lea di, [volume_label]
pvl_copy:
    lodsb
    call up_al
    stosb
    loop pvl_copy
pvl_done:
    pop es
    pop di
    pop si
    pop cx
    pop ax
    ret

/* Apply classic floppy geometry switches after BIOS probing. */
apply_presets:
    push ax
    push dx
    mov al, [flag_f720]
    and al, [flag_four]
    jnz ap_fail
    mov al, [flag_f720]
    or al, [flag_four]
    or al, [flag_one]
    jz ap_ok
    cmp byte ptr [drive_dl], 0x80
    jae ap_fail

    cmp byte ptr [flag_f720], 0
    je ap_four
    mov word ptr [bpb_spt], 9
    mov word ptr [bpb_heads], 2
    mov word ptr [bpb_totsec], 1440
    mov word ptr [bpb_totsec_hi], 0
    mov byte ptr [preset_media], 0xF9
    jmp ap_one

ap_four:
    cmp byte ptr [flag_four], 0
    je ap_one
    mov word ptr [bpb_spt], 9
    mov word ptr [bpb_heads], 2
    mov word ptr [bpb_totsec], 720
    mov word ptr [bpb_totsec_hi], 0
    mov byte ptr [preset_media], 0xFD

ap_one:
    cmp byte ptr [flag_one], 0
    je ap_ok
    cmp word ptr [bpb_heads], 1
    jbe ap_one_media
    mov word ptr [bpb_heads], 1
    shr word ptr [bpb_totsec_hi], 1
    rcr word ptr [bpb_totsec], 1
ap_one_media:
    cmp byte ptr [flag_four], 0
    je ap_ok
    mov byte ptr [preset_media], 0xFC
ap_ok:
    pop dx
    pop ax
    clc
    ret
ap_fail:
    pop dx
    pop ax
    stc
    ret

get_geometry:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds
    pop es
    mov word ptr [bpb_totsec_hi], 0
    mov word ptr [vol_base_lba], 0
    /* Cache CHS early so EBR walks use HD geometry (not floppy 9×2 default). */
    mov ah, 0x08
    mov dl, [drive_dl]
    int 0x13
    jc gg_chs_early_fb
    mov al, cl
    and ax, 0x3F
    test ax, ax
    jz gg_chs_early_fb
    mov [bpb_spt], ax
    mov al, dh
    inc ax
    and ax, 0x00FF
    mov [bpb_heads], ax
    jmp gg_chs_early_done
gg_chs_early_fb:
    mov word ptr [bpb_spt], 17
    mov word ptr [bpb_heads], 4
gg_chs_early_done:
    mov dl, [drive_dl]
    mov ax, 0
    lea bx, [secbuf]
    call read_raw_lba
    jc gg_bios
    /* Nth DOS volume (C:=0, D:=1, …): primaries then extended logicals. */
    cmp word ptr [secbuf + 510], 0xAA55
    jne gg_vbr
    mov dl, [drive_idx]
    cmp dl, 2
    jb gg_vbr
    sub dl, 2
    mov si, 0x1BE
    mov cx, 4
gg_part:
    mov al, [secbuf + si + 4]
    cmp al, 0x01
    je gg_dos_part
    cmp al, 0x04
    je gg_dos_part
    cmp al, 0x06
    jne gg_part_next
gg_dos_part:
    cmp word ptr [secbuf + si + 10], 0
    jne gg_fail
    mov ax, [secbuf + si + 8]
    test ax, ax
    jz gg_part_next
    test dl, dl
    jnz gg_part_skip
    mov [vol_base_lba], ax
    mov ax, [secbuf + si + 12]
    mov dx, [secbuf + si + 14]
    mov [bpb_totsec], ax
    mov [bpb_totsec_hi], dx
    jmp gg_chs_only
gg_part_skip:
    dec dl
gg_part_next:
    add si, 16
    loop gg_part

    /* Collect extended bases, then walk for remaining index in DL */
    mov word ptr [fmt_ext0], 0
    mov word ptr [fmt_ext1], 0
    xor di, di
    mov si, 0x1BE
    mov cx, 4
gg_col:
    mov al, [secbuf + si + 4]
    cmp al, 0x05
    je gg_col_ext
    cmp al, 0x0F
    jne gg_col_n
gg_col_ext:
    cmp word ptr [secbuf + si + 10], 0
    jne gg_col_n
    mov ax, [secbuf + si + 8]
    test ax, ax
    jz gg_col_n
    cmp di, 4
    jae gg_col_n
    mov [fmt_ext0 + di], ax
    add di, 2
gg_col_n:
    add si, 16
    loop gg_col

    xor di, di
    mov cx, 2
gg_walk_exts:
    mov ax, [fmt_ext0 + di]
    test ax, ax
    jz gg_we_next
    mov [fmt_ext_base], ax
    call fmt_walk_logicals
    jc gg_chs_only                 /* CF set → found; vol_base/totsec set */
gg_we_next:
    add di, 2
    loop gg_walk_exts
    /* HD letter requested but no matching volume */
    jmp gg_fail

/*
 * Walk extended at fmt_ext_base. DL = remaining volume index.
 * On match: set vol_base_lba/totsec, CF=1. Else CF=0, DL updated.
 * Clobbers secbuf.
 */
fmt_walk_logicals:
    push ax
    push bx
    push cx
    push si
    mov ax, [fmt_ext_base]
    mov [fmt_ebr_lba], ax
.fwl_loop:
    mov ax, [fmt_ebr_lba]
    lea bx, [secbuf]
    call read_raw_lba
    jc .fwl_miss
    cmp word ptr [secbuf + 510], 0xAA55
    jne .fwl_miss
    mov al, [secbuf + 0x1BE + 4]
    cmp al, 0x01
    je .fwl_dos
    cmp al, 0x04
    je .fwl_dos
    cmp al, 0x06
    jne .fwl_link
.fwl_dos:
    cmp word ptr [secbuf + 0x1BE + 10], 0
    jne .fwl_link
    mov ax, [fmt_ebr_lba]
    add ax, [secbuf + 0x1BE + 8]
    jc .fwl_link
    test dl, dl
    jnz .fwl_skip
    mov [vol_base_lba], ax
    mov ax, [secbuf + 0x1BE + 12]
    mov bx, [secbuf + 0x1BE + 14]
    mov [bpb_totsec], ax
    mov [bpb_totsec_hi], bx
    stc
    jmp .fwl_ret
.fwl_skip:
    dec dl
.fwl_link:
    mov al, [secbuf + 0x1CE + 4]
    cmp al, 0x05
    je .fwl_next
    cmp al, 0x0F
    jne .fwl_miss
.fwl_next:
    mov ax, [fmt_ext_base]
    add ax, [secbuf + 0x1CE + 8]
    jc .fwl_miss
    cmp ax, [fmt_ebr_lba]
    je .fwl_miss
    mov [fmt_ebr_lba], ax
    jmp .fwl_loop
.fwl_miss:
    clc
.fwl_ret:
    pop si
    pop cx
    pop bx
    pop ax
    ret

gg_vbr:
    cmp word ptr [secbuf + 11], 512
    jne gg_bios
    mov ax, [secbuf + 19]
    test ax, ax
    jnz gg_tot16
    mov ax, [secbuf + 32]
    mov dx, [secbuf + 34]
    mov [bpb_totsec], ax
    mov [bpb_totsec_hi], dx
    jmp gg_tot_chk
gg_tot16:
    mov [bpb_totsec], ax
gg_tot_chk:
    mov ax, [secbuf + 24]
    test ax, ax
    jz gg_bios
    mov [bpb_spt], ax
    mov ax, [secbuf + 26]
    test ax, ax
    jz gg_bios
    mov [bpb_heads], ax
    jmp gg_cap
gg_chs_only:
    /* Partition size already set; only fetch SPT/heads from BIOS. */
    mov ah, 0x08
    mov dl, [drive_dl]
    int 0x13
    jc gg_chs_fb
    mov al, cl
    and ax, 0x3F
    test ax, ax
    jz gg_chs_fb
    mov [bpb_spt], ax
    mov al, dh
    inc ax
    and ax, 0x00FF
    mov [bpb_heads], ax
    jmp gg_cap
gg_chs_fb:
    mov word ptr [bpb_spt], 17
    mov word ptr [bpb_heads], 4
    jmp gg_cap
gg_bios:
    mov ah, 0x08
    mov dl, [drive_dl]
    int 0x13
    jc gg_fb
    mov al, cl
    and ax, 0x3F
    test ax, ax
    jz gg_fb
    mov [bpb_spt], ax
    mov al, dh
    inc ax
    and ax, 0x00FF
    mov [bpb_heads], ax
    mov al, ch
    mov ah, cl
    mov cl, 6
    shr ah, cl
    inc ax                       /* cylinders */
    mul word ptr [bpb_heads]
    mul word ptr [bpb_spt]       /* DX:AX = totsec */
    mov [bpb_totsec], ax
    mov [bpb_totsec_hi], dx
    mov ax, dx
    or ax, [bpb_totsec]
    jz gg_fb
    jmp gg_cap
gg_fb:
    mov word ptr [bpb_spt], 9
    mov word ptr [bpb_heads], 2
    mov word ptr [bpb_totsec], 1440
    mov word ptr [bpb_totsec_hi], 0
gg_cap:
    /* refuse > 128 MiB (262144 = 0x40000 sectors) */
    mov ax, [bpb_totsec_hi]
    cmp ax, 4
    ja gg_fail
    jb gg_ok
    cmp word ptr [bpb_totsec], 0
    ja gg_fail
gg_ok:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
gg_fail:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

compute_layout:
    push ax
    push bx
    push cx
    push dx
    mov byte ptr [bpb_fats], 2
    mov byte ptr [bpb_spc], 1
    mov byte ptr [fat_type], 12
    /* HD vs floppy defaults */
    mov ax, [bpb_totsec_hi]
    test ax, ax
    jnz cl_hd
    cmp word ptr [bpb_totsec], 2880
    ja cl_hd
    mov word ptr [bpb_root_ents], 112
    cmp word ptr [bpb_totsec_hi], 0
    jne cl_floppy_root
    cmp word ptr [bpb_totsec], 360
    ja cl_floppy_root
    mov word ptr [bpb_root_ents], 64
cl_floppy_root:
    mov word ptr [bpb_reserved], 2
    mov bl, 0xF9
    cmp word ptr [bpb_totsec], 2400
    jbe cl_med
    mov bl, 0xF0
    jmp cl_med
cl_hd:
    mov word ptr [bpb_root_ents], 512
    mov word ptr [bpb_reserved], 2
    mov bl, 0xF8
cl_med:
    cmp byte ptr [preset_media], 0
    je cl_med_store
    mov bl, [preset_media]
cl_med_store:
    mov [bpb_media], bl
cl_spc:
    mov ax, [bpb_root_ents]
    mov cl, 4
    shr ax, cl
    mov [bpb_root_secs], ax
    mov word ptr [bpb_spf], 1
cl_spf:
    mov ax, [bpb_reserved]
    mov [bpb_fat1_lba], ax
    mov bx, [bpb_spf]
    add ax, bx
    mov [bpb_fat2_lba], ax
    add ax, bx
    add ax, [bpb_root_secs]
    mov [bpb_data_lba], ax
    /* clusters = (totsec - data_lba) / spc */
    mov bx, ax
    mov ax, [bpb_totsec]
    mov dx, [bpb_totsec_hi]
    sub ax, bx
    sbb dx, 0
    mov bl, [bpb_spc]
    xor bh, bh
    test bx, bx
    jz cl_fail
    div bx                       /* AX = cluster count for era sizes */
    mov [clust_cnt], ax
    cmp ax, 4085
    jb cl_fat12
    /* Prefer FAT12 by growing SPC up to 8; then FAT16 */
    cmp byte ptr [bpb_spc], 8
    jb cl_bump
    cmp ax, 65525
    ja cl_bump
    mov byte ptr [fat_type], 16
    mov bx, ax
    add bx, 2
    shl bx, 1                    /* fat bytes = (clust+2)*2 */
    mov ax, bx
    add ax, 511
    mov cl, 9
    shr ax, cl
    cmp ax, [bpb_spf]
    jbe cl_ok
    mov [bpb_spf], ax
    jmp cl_spf
cl_fat12:
    mov byte ptr [fat_type], 12
    mov bx, ax
    add bx, 2
    mov ax, bx
    add ax, bx
    add ax, bx
    inc ax
    shr ax, 1                    /* fat bytes */
    add ax, 511
    mov cl, 9
    shr ax, cl
    cmp ax, [bpb_spf]
    jbe cl_ok
    mov [bpb_spf], ax
    jmp cl_spf
cl_ok:
    mov ax, [clust_cnt]
    add ax, 2
    mov [bpb_max_clust], ax
    mov ax, [bpb_fat2_lba]
    add ax, [bpb_spf]
    mov [bpb_root_lba], ax
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
cl_bump:
    mov al, [bpb_spc]
    cmp al, 64
    jae cl_fail
    shl al, 1
    mov [bpb_spc], al
    jmp cl_spc
cl_fail:
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

/* AX = physical disk LBA, ES:BX = buffer. */
read_raw_lba:
    push ax
    push bx
    push cx
    push dx
    push si
    push es
    push ds
    pop es
    mov si, bx
    xor dx, dx
    mov cx, [bpb_spt]
    test cx, cx
    jnz rl_spt
    mov cx, 9
rl_spt:
    div cx
    mov cl, dl
    inc cl
    xor dx, dx
    mov bx, [bpb_heads]
    test bx, bx
    jnz rl_hd
    mov bx, 2
rl_hd:
    div bx
    mov dh, dl
    mov ch, al
    mov al, ah
    mov ah, cl
    mov cl, 6
    shl al, cl
    or al, ah
    mov cl, al
    mov dl, [drive_dl]
    mov bx, si
    mov ah, 0x02
    mov al, 1
    int 0x13
    pop es
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* AX = volume-relative LBA, ES:BX = buffer. Preserves relative AX. */
read_lba:
    add ax, [vol_base_lba]
    jmp read_raw_lba

write_lba:
    push ax
    add ax, [vol_base_lba]
    call write_raw_lba
    pop ax
    ret

/* AX = absolute LBA, ES:BX = buffer. */
write_raw_lba:
    push ax
    push bx
    push cx
    push dx
    push si
    push es
    push ds
    pop es
    mov si, bx
    xor dx, dx
    mov cx, [bpb_spt]
    test cx, cx
    jnz wl_spt
    mov cx, 9
wl_spt:
    div cx
    mov cl, dl
    inc cl
    xor dx, dx
    mov bx, [bpb_heads]
    test bx, bx
    jnz wl_hd
    mov bx, 2
wl_hd:
    div bx
    mov dh, dl
    mov ch, al
    mov al, ah
    mov ah, cl
    mov cl, 6
    shl al, cl
    or al, ah
    mov cl, al
    mov dl, [drive_dl]
    mov bx, si
    mov ah, 0x03
    mov al, 1
    int 0x13
    pop es
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

apply_bpb:
    push si
    push di
    mov word ptr [secbuf + 11], 512
    mov al, [bpb_spc]
    mov [secbuf + 13], al
    mov ax, [bpb_reserved]
    mov [secbuf + 14], ax
    mov al, [bpb_fats]
    mov [secbuf + 16], al
    mov ax, [bpb_root_ents]
    mov [secbuf + 17], ax
    mov ax, [bpb_totsec_hi]
    test ax, ax
    jnz ab_ts32
    mov ax, [bpb_totsec]
    mov [secbuf + 19], ax
    xor ax, ax
    mov [secbuf + 32], ax
    mov [secbuf + 34], ax
    jmp ab_ts_done
ab_ts32:
    mov word ptr [secbuf + 19], 0
    mov ax, [bpb_totsec]
    mov [secbuf + 32], ax
    mov ax, [bpb_totsec_hi]
    mov [secbuf + 34], ax
ab_ts_done:
    mov al, [bpb_media]
    mov [secbuf + 21], al
    mov ax, [bpb_spf]
    mov [secbuf + 22], ax
    mov ax, [bpb_spt]
    mov [secbuf + 24], ax
    mov ax, [bpb_heads]
    mov [secbuf + 26], ax
    mov ax, [vol_base_lba]
    mov [secbuf + 28], ax
    xor ax, ax
    mov [secbuf + 30], ax
    mov al, [drive_dl]
    mov [secbuf + 36], al
    mov byte ptr [secbuf + 38], 0x29
    /* OEM "rmDOS   " at +3 */
    mov word ptr [secbuf + 3], 0x6D72        /* "rm" */
    mov word ptr [secbuf + 5], 0x4F44        /* "DO" */
    mov word ptr [secbuf + 7], 0x2053        /* "S " */
    mov word ptr [secbuf + 9], 0x2020        /* "  " */
    /* Volume label at +43; FS type at +54 */
    push es
    push cx
    push ax
    push ds
    pop es
    lea di, [secbuf + 43]
    lea si, [volume_label]
    mov cx, 11
    rep movsb
    pop ax
    pop cx
    pop es
    cmp byte ptr [fat_type], 16
    je ab_fat16
    mov word ptr [secbuf + 54], 0x4146       /* "FA" */
    mov word ptr [secbuf + 56], 0x3154       /* "T1" */
    mov word ptr [secbuf + 58], 0x2032       /* "2 " */
    mov word ptr [secbuf + 60], 0x2020       /* "  " */
    jmp ab_fs_done
ab_fat16:
    mov word ptr [secbuf + 54], 0x4146       /* "FA" */
    mov word ptr [secbuf + 56], 0x3154       /* "T1" */
    mov word ptr [secbuf + 58], 0x2036       /* "6 " */
    mov word ptr [secbuf + 60], 0x2020       /* "  " */
ab_fs_done:
    pop di
    pop si
    ret

/*
 * If formatting a DOS volume (vol_base_lba != 0), set partition type from FS
 * in the MBR primary entry or the EBR logical entry that matches the base.
 */
update_part_type:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov ax, [vol_base_lba]
    test ax, ax
    jz upt_done
    push ds
    pop es
    xor ax, ax
    lea bx, [secbuf]
    call read_raw_lba
    jc upt_done
    cmp word ptr [secbuf + 510], 0xAA55
    jne upt_done
    mov si, 0x1BE
    mov cx, 4
    mov dx, [vol_base_lba]
upt_scan:
    cmp word ptr [secbuf + si + 10], 0
    jne upt_next
    cmp [secbuf + si + 8], dx
    jne upt_try_ext
    call upt_set_type
    xor ax, ax
    lea bx, [secbuf]
    call write_raw_lba
    jmp upt_done
upt_try_ext:
    mov al, [secbuf + si + 4]
    cmp al, 0x05
    je upt_ext
    cmp al, 0x0F
    jne upt_next
upt_ext:
    mov ax, [secbuf + si + 8]
    test ax, ax
    jz upt_next
    push si
    push cx
    mov [fmt_ext_base], ax
    call upt_logical
    pop cx
    pop si
    jc upt_done
upt_next:
    add si, 16
    loop upt_scan
upt_done:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* Set type byte at [secbuf+si+4] from fat_type / size. */
upt_set_type:
    mov al, 0x01
    cmp byte ptr [fat_type], 16
    jne .ust
    mov al, 0x04
    mov bx, [bpb_totsec_hi]
    test bx, bx
    jnz .ust6
    cmp word ptr [bpb_totsec], 65535
    jbe .ust
.ust6:
    mov al, 0x06
.ust:
    mov [secbuf + si + 4], al
    ret

/* Find logical with absolute base == vol_base_lba; update EBR. CF if done. */
upt_logical:
    push ax
    push bx
    push cx
    push dx
    push si
    mov ax, [fmt_ext_base]
    mov [fmt_ebr_lba], ax
    mov dx, [vol_base_lba]
.ul_loop:
    mov ax, [fmt_ebr_lba]
    lea bx, [secbuf]
    call read_raw_lba
    jc .ul_miss
    cmp word ptr [secbuf + 510], 0xAA55
    jne .ul_miss
    mov ax, [fmt_ebr_lba]
    add ax, [secbuf + 0x1BE + 8]
    cmp ax, dx
    jne .ul_link
    lea si, [secbuf + 0x1BE]
    call upt_set_type
    mov ax, [fmt_ebr_lba]
    lea bx, [secbuf]
    call write_raw_lba
    stc
    jmp .ul_ret
.ul_link:
    mov al, [secbuf + 0x1CE + 4]
    cmp al, 0x05
    je .ul_next
    cmp al, 0x0F
    jne .ul_miss
.ul_next:
    mov ax, [fmt_ext_base]
    add ax, [secbuf + 0x1CE + 8]
    cmp ax, [fmt_ebr_lba]
    je .ul_miss
    mov [fmt_ebr_lba], ax
    jmp .ul_loop
.ul_miss:
    clc
.ul_ret:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

format_disk:
    push ax
    push bx
    push cx
    mov ax, 0
    lea bx, [secbuf]
    call read_lba
    jnc fmt_got
    call clear_buf
    mov word ptr [secbuf], 0x3CEB
    mov byte ptr [secbuf + 2], 0x90
fmt_got:
    call apply_bpb
    mov word ptr [secbuf + 510], 0xAA55
    mov ax, 0
    lea bx, [secbuf]
    call write_lba
    jc fmt_fail
    cmp word ptr [bpb_reserved], 2
    jb fmt_fats
    call clear_buf
    mov ax, 1
    lea bx, [secbuf]
    call write_lba
    jc fmt_fail
fmt_fats:
    call write_empty_fats
    jc fmt_fail
    call clear_buf
    mov cx, [bpb_root_secs]
    mov ax, [bpb_root_lba]
fmt_root:
    push ax
    push cx
    lea bx, [secbuf]
    call write_lba
    pop cx
    pop ax
    jc fmt_fail
    inc ax
    loop fmt_root
    clc
    jmp fmt_done
fmt_fail:
    stc
fmt_done:
    pop cx
    pop bx
    pop ax
    ret

/* Add /V label as a root-directory volume entry after optional /S files. */
write_volume_label:
    push ax
    push bx
    push cx
    push si
    push di
    push es
    cmp byte ptr [flag_v], 0
    je wvl_ok

    lea si, [volume_label]
    mov cx, 11
wvl_nonempty:
    cmp byte ptr [si], ' '
    jne wvl_read
    inc si
    loop wvl_nonempty
    jmp wvl_ok

wvl_read:
    push ds
    pop es
    mov ax, [bpb_root_lba]
    lea bx, [secbuf]
    call read_lba
    jc wvl_fail
    lea di, [secbuf]
    mov cx, 16
wvl_find:
    cmp byte ptr [di], 0
    je wvl_slot
    cmp byte ptr [di], 0xE5
    je wvl_slot
    add di, 32
    loop wvl_find
    jmp wvl_fail

wvl_slot:
    lea si, [volume_label]
    mov cx, 11
    rep movsb
    mov al, 0x08
    stosb
    xor ax, ax
    mov cx, 10
    rep stosw
    mov ax, [bpb_root_lba]
    lea bx, [secbuf]
    call write_lba
    jc wvl_fail
wvl_ok:
    pop es
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    clc
    ret
wvl_fail:
    pop es
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    stc
    ret

write_empty_fats:
    push ax
    push bx
    push cx
    call clear_buf
    mov al, [bpb_media]
    mov [secbuf], al
    mov byte ptr [secbuf + 1], 0xFF
    mov byte ptr [secbuf + 2], 0xFF
    cmp byte ptr [fat_type], 16
    jne wef_media_done
    mov byte ptr [secbuf + 3], 0xFF
wef_media_done:
    mov ax, [bpb_fat1_lba]
    lea bx, [secbuf]
    call write_lba
    jc wef_fail
    mov ax, [bpb_fat2_lba]
    lea bx, [secbuf]
    call write_lba
    jc wef_fail
    call clear_buf
    mov cx, [bpb_spf]
    dec cx
    jz wef_ok
    mov ax, [bpb_fat1_lba]
    inc ax
wef1:
    push ax
    push cx
    lea bx, [secbuf]
    call write_lba
    pop cx
    pop ax
    jc wef_fail
    inc ax
    loop wef1
    mov cx, [bpb_spf]
    dec cx
    mov ax, [bpb_fat2_lba]
    inc ax
wef2:
    push ax
    push cx
    lea bx, [secbuf]
    call write_lba
    pop cx
    pop ax
    jc wef_fail
    inc ax
    loop wef2
wef_ok:
    clc
    jmp wef_done
wef_fail:
    stc
wef_done:
    pop cx
    pop bx
    pop ax
    ret

load_one:
    /* DX = name; out: AX=seg CX=size */
    push bx
    push dx
    mov ah, 0x3D
    xor al, al
    int 0x21
    jc lo_fail
    mov [tmp_h], bx
    mov ah, 0x42
    mov al, 2
    xor cx, cx
    xor dx, dx
    int 0x21
    jc lo_fail
    mov [tmp_size], ax
    test ax, ax
    jz lo_fail
    mov bx, ax
    add bx, 15
    mov cl, 4
    shr bx, cl
    mov ah, 0x48
    int 0x21
    jc lo_fail
    mov [tmp_seg], ax
    mov ah, 0x42
    mov al, 0
    mov bx, [tmp_h]
    xor cx, cx
    xor dx, dx
    int 0x21
    mov ah, 0x3F
    mov bx, [tmp_h]
    mov cx, [tmp_size]
    push ds
    mov ds, [tmp_seg]
    xor dx, dx
    int 0x21
    pop ds
    jc lo_fail
    mov ah, 0x3E
    mov bx, [tmp_h]
    int 0x21
    mov ax, [tmp_seg]
    mov cx, [tmp_size]
    pop dx
    pop bx
    clc
    ret
lo_fail:
    pop dx
    pop bx
    stc
    ret

load_system:
    lea dx, [name_kern]
    call load_one
    jc ls_bad
    mov [kern_seg], ax
    mov [kern_size], cx
    lea dx, [name_cmd]
    call load_one
    jc ls_bad
    mov [cmd_seg], ax
    mov [cmd_size], cx
    mov ax, 0
    lea bx, [boot_tmpl]
    call read_lba
    jc ls_bad
    clc
    ret
ls_bad:
    stc
    ret

free_system:
    cmp word ptr [kern_seg], 0
    je fr_c
    push es
    mov es, [kern_seg]
    mov ah, 0x49
    int 0x21
    pop es
    mov word ptr [kern_seg], 0
fr_c:
    cmp word ptr [cmd_seg], 0
    je fr_f
    push es
    mov es, [cmd_seg]
    mov ah, 0x49
    int 0x21
    pop es
    mov word ptr [cmd_seg], 0
fr_f:
    cmp word ptr [fat_seg], 0
    je fr_d
    push es
    mov es, [fat_seg]
    mov ah, 0x49
    int 0x21
    pop es
    mov word ptr [fat_seg], 0
fr_d:
    ret

fill_from:
    /* BX=seg SI=off -> secbuf */
    push ax
    push cx
    push si
    push di
    push ds
    push es
    call clear_buf
    push cs
    pop es
    lea di, [secbuf]
    mov ds, bx
    mov cx, 256
    rep movsw
    pop es
    pop ds
    pop di
    pop si
    pop cx
    pop ax
    ret

write_system:
    mov ax, [kern_size]
    add ax, 511
    mov cl, 9
    shr ax, cl
    mov [kern_secs], ax
    mov ax, [cmd_size]
    add ax, 511
    mov cl, 9
    shr ax, cl
    mov [cmd_secs], ax
    /* cluster counts = ceil(secs / spc) */
    mov bl, [bpb_spc]
    xor bh, bh
    mov ax, [kern_secs]
    add ax, bx
    dec ax
    xor dx, dx
    div bx
    mov [kern_nclust], ax
    mov ax, [cmd_secs]
    add ax, bx
    dec ax
    xor dx, dx
    div bx
    mov [cmd_nclust], ax
    mov word ptr [kern_clust], 2
    mov ax, 2
    add ax, [kern_nclust]
    mov [cmd_clust], ax
    mov ax, [bpb_data_lba]
    mov [kern_lba], ax
    /* cmd_lba = data + kern_nclust * spc */
    mov ax, [kern_nclust]
    mul bx
    add ax, [bpb_data_lba]
    mov [cmd_lba], ax
    xor si, si
    mov cx, [kern_secs]
    mov ax, [kern_lba]
ws_k:
    push ax
    push cx
    push si
    mov bx, [kern_seg]
    call fill_from
    pop si
    pop cx
    pop ax
    push ax
    push cx
    push si
    lea bx, [secbuf]
    call write_lba
    pop si
    add si, 512
    pop cx
    pop ax
    jc ws_bad
    inc ax
    loop ws_k
    /* pad to cluster boundary before COMMAND */
    mov ax, [cmd_lba]
    xor si, si
    mov cx, [cmd_secs]
ws_c:
    push ax
    push cx
    push si
    mov bx, [cmd_seg]
    call fill_from
    pop si
    pop cx
    pop ax
    push ax
    push cx
    push si
    lea bx, [secbuf]
    call write_lba
    pop si
    add si, 512
    pop cx
    pop ax
    jc ws_bad
    inc ax
    loop ws_c
    call build_fat_sys
    jc ws_bad
    call write_root_sys
    jc ws_bad
    call write_rfat1
    jc ws_bad
    push ds
    push es
    push cs
    pop ds
    push cs
    pop es
    lea si, [boot_tmpl]
    lea di, [secbuf]
    mov cx, 256
    rep movsw
    pop es
    pop ds
    call apply_bpb
    mov word ptr [secbuf + 510], 0xAA55
    mov ax, 0
    lea bx, [secbuf]
    call write_lba
    jc ws_bad
    clc
    ret
ws_bad:
    stc
    ret

build_fat_sys:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov bx, [bpb_spf]
    mov cl, 5
    shl bx, cl
    mov ah, 0x48
    int 0x21
    jc bfs_bad
    mov [fat_seg], ax
    mov es, ax
    xor di, di
    mov dx, [bpb_spf]
    mov cl, 8
    shl dx, cl                   /* words to clear = spf * 256 */
    mov cx, dx
    xor ax, ax
    rep stosw
    mov al, [bpb_media]
    mov es:[0], al
    mov byte ptr es:[1], 0xFF
    mov byte ptr es:[2], 0xFF
    cmp byte ptr [fat_type], 16
    jne bfs_media_ok
    mov byte ptr es:[3], 0xFF
bfs_media_ok:
    mov ax, [kern_clust]
    mov cx, [kern_nclust]
    call fat_chain
    mov ax, [cmd_clust]
    mov cx, [cmd_nclust]
    call fat_chain
    xor si, si
    mov cx, [bpb_spf]
    mov ax, [bpb_fat1_lba]
    call write_fat_copy
    jc bfs_bad2
    xor si, si
    mov cx, [bpb_spf]
    mov ax, [bpb_fat2_lba]
    call write_fat_copy
    jc bfs_bad2
    mov es, [fat_seg]
    mov ah, 0x49
    int 0x21
    mov word ptr [fat_seg], 0
    clc
    jmp bfs_done
bfs_bad2:
    mov es, [fat_seg]
    mov ah, 0x49
    int 0x21
    mov word ptr [fat_seg], 0
bfs_bad:
    stc
bfs_done:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

write_fat_copy:
wfc_lp:
    push ax
    push cx
    push si
    push ds
    push es
    push cs
    pop es
    lea di, [secbuf]
    mov ds, [fat_seg]
    mov cx, 256
    rep movsw
    pop es
    pop ds
    pop si
    add si, 512
    pop cx
    pop ax
    push ax
    push cx
    push si
    lea bx, [secbuf]
    call write_lba
    pop si
    pop cx
    pop ax
    jc wfc_bad
    inc ax
    loop wfc_lp
    clc
    ret
wfc_bad:
    stc
    ret

fat_chain:
    push ax
    push bx
    push cx
fc_lp:
    cmp cx, 1
    jbe fc_last
    mov bx, ax
    inc bx
    call fat_set
    inc ax
    dec cx
    jmp fc_lp
fc_last:
    mov bx, 0xFFFF
    cmp byte ptr [fat_type], 16
    je fc_eoc
    mov bx, 0x0FFF
fc_eoc:
    call fat_set
    pop cx
    pop bx
    pop ax
    ret

fat_set:
    push ax
    push bx
    push cx
    push si
    push es
    mov es, [fat_seg]
    cmp byte ptr [fat_type], 16
    je fs16
    mov si, ax
    shr si, 1
    add si, ax
    test al, 1
    jnz fso
    mov ax, es:[si]
    and ax, 0xF000
    and bx, 0x0FFF
    or ax, bx
    mov es:[si], ax
    jmp fsd
fso:
    mov ax, es:[si]
    and ax, 0x000F
    mov cl, 4
    shl bx, cl
    or ax, bx
    mov es:[si], ax
    jmp fsd
fs16:
    mov si, ax
    shl si, 1
    mov es:[si], bx
fsd:
    pop es
    pop si
    pop cx
    pop bx
    pop ax
    ret

write_root_sys:
    push ax
    push bx
    push cx
    push si
    push di
    push es
    call clear_buf
    push cs
    pop es
    lea di, [secbuf]
    lea si, [nm_kern]
    mov cx, 11
    rep movsb
    mov al, 0x20
    stosb
    xor ax, ax
    mov cx, 7                   /* reserved+time+date = 14 bytes */
    rep stosw
    mov ax, [kern_clust]
    stosw
    mov ax, [kern_size]
    stosw
    xor ax, ax
    stosw
    lea si, [nm_cmd]
    mov cx, 11
    rep movsb
    mov al, 0x20
    stosb
    xor ax, ax
    mov cx, 7
    rep stosw
    mov ax, [cmd_clust]
    stosw
    mov ax, [cmd_size]
    stosw
    xor ax, ax
    stosw
    mov ax, [bpb_root_lba]
    lea bx, [secbuf]
    call write_lba
    pop es
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    ret

write_rfat1:
    push ax
    push bx
    call clear_buf
    mov byte ptr [secbuf], 'R'
    mov byte ptr [secbuf + 1], 'F'
    mov byte ptr [secbuf + 2], 'A'
    mov byte ptr [secbuf + 3], 'T'
    mov byte ptr [secbuf + 4], '1'
    mov byte ptr [secbuf + 5], 1
    mov ax, [kern_lba]
    mov [secbuf + 0x1C], ax
    mov ax, [kern_secs]
    mov [secbuf + 0x1E], ax
    mov ax, 1
    lea bx, [secbuf]
    call write_lba
    pop bx
    pop ax
    ret

flag_y:
    .byte 0
flag_s:
    .byte 0
flag_v:
    .byte 0
flag_f720:
    .byte 0
flag_one:
    .byte 0
flag_four:
    .byte 0
preset_media:
    .byte 0
drive_dl:
    .byte 0
drive_let:
    .byte 'A'
drive_idx:
    .byte 0
vol_base_lba:
    .word 0
fmt_ext_base:
    .word 0
fmt_ebr_lba:
    .word 0
fmt_ext0:
    .word 0
fmt_ext1:
    .word 0
bpb_spc:
    .byte 1
bpb_fats:
    .byte 2
bpb_media:
    .byte 0xF9
bpb_reserved:
    .word 2
bpb_root_ents:
    .word 112
bpb_totsec:
    .word 1440
bpb_totsec_hi:
    .word 0
bpb_spf:
    .word 3
bpb_spt:
    .word 9
bpb_heads:
    .word 2
bpb_fat1_lba:
    .word 2
bpb_fat2_lba:
    .word 5
bpb_root_lba:
    .word 8
bpb_root_secs:
    .word 7
bpb_data_lba:
    .word 15
bpb_max_clust:
    .word 0x592
fat_type:
    .byte 12
clust_cnt:
    .word 0
kern_seg:
    .word 0
cmd_seg:
    .word 0
fat_seg:
    .word 0
kern_size:
    .word 0
cmd_size:
    .word 0
kern_secs:
    .word 0
cmd_secs:
    .word 0
kern_nclust:
    .word 0
cmd_nclust:
    .word 0
kern_clust:
    .word 0
cmd_clust:
    .word 0
kern_lba:
    .word 0
cmd_lba:
    .word 0
tmp_h:
    .word 0
tmp_seg:
    .word 0
tmp_size:
    .word 0
nm_kern:
    .ascii "KERNEL  SYS"
nm_cmd:
    .ascii "COMMAND COM"
name_kern:
    .asciz "KERNEL.SYS"
name_cmd:
    .asciz "COMMAND.COM"
volume_label:
    .space 11, ' '
label_input:
    .byte 11, 0
    .space 11, 0
msg_warn1:
    .ascii "WARNING: ALL data on $"
msg_warn2:
    .ascii ": will be lost!\r\nProceed (Y/N)? $"
msg_fmt:
    .ascii "Formatting...\r\n$"
msg_load:
    .ascii "Loading system...\r\n$"
msg_ok:
    .ascii "Format complete\r\nFORMAT OK\r\n$"
msg_sys:
    .ascii "System transferred\r\n$"
msg_abort:
    .ascii "Format aborted\r\n$"
msg_err:
    .ascii "Format failed\r\n$"
msg_geo:
    .ascii "Bad geometry\r\n$"
msg_u:
    .ascii "FORMAT [d:] [/S] [/Y] [/V[:label]] [/F:720] [/1] [/4]\r\n$"
msg_label:
    .ascii "Volume label (11 characters, ENTER for none)? $"
msg_crlf:
    .ascii "\r\n$"
boot_tmpl:
    .space 512, 0
secbuf:
    .space 512, 0
