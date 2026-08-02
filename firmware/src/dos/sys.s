.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * SYS [d:]
 *
 * Copy KERNEL.SYS and COMMAND.COM from the current drive to the target, then
 * install the rmDOS boot code and regenerate RFAT1 from KERNEL.SYS' target
 * cluster chain. The target FAT, BPB, and unrelated root entries are retained.
 */
_start:
    push cs
    pop ds
    push cs
    pop es
    mov word ptr [src_h], 0xFFFF
    mov word ptr [dst_h], 0xFFFF

    mov ah, 0x19
    int 0x21
    mov [source_drive], al
    mov byte ptr [target_drive], 0

    mov si, 0x81
.arg_skip:
    cmp byte ptr [si], ' '
    je .arg_inc
    cmp byte ptr [si], 9
    jne .arg_check
.arg_inc:
    inc si
    jmp .arg_skip
.arg_check:
    mov al, [si]
    cmp al, 0x0D
    je .args_done
    test al, al
    jz .args_done
    and al, 0xDF
    cmp al, 'A'
    jb usage
    cmp al, 'Z'
    ja usage
    cmp byte ptr [si + 1], ':'
    jne usage
    sub al, 'A'
    mov [target_drive], al
    add si, 2
.arg_tail:
    cmp byte ptr [si], ' '
    je .arg_tail_inc
    cmp byte ptr [si], 9
    je .arg_tail_inc
    cmp byte ptr [si], 0x0D
    je .args_done
    cmp byte ptr [si], 0
    je .args_done
    jmp usage
.arg_tail_inc:
    inc si
    jmp .arg_tail

.args_done:
    call build_paths

    /* Boot code comes from the current/source volume. */
    mov al, [source_drive]
    mov cx, 1
    xor dx, dx
    lea bx, [boot_src]
    call abs_read
    jc fail
    cmp word ptr [boot_src + 510], 0xAA55
    jne fail

    mov ah, 0x0D
    int 0x21
    mov al, [target_drive]
    mov cx, 1
    xor dx, dx
    lea bx, [boot_dst]
    call abs_read
    jc fail
    cmp word ptr [boot_dst + 11], 512
    jne fail
    cmp word ptr [boot_dst + 14], 2
    jb fail

    /* Avoid truncating the source when SYS targets the current volume. */
    mov al, [source_drive]
    cmp al, [target_drive]
    je .files_ready
    lea si, [src_kern]
    lea di, [dst_kern]
    call copy_one
    jc fail
    lea si, [src_cmd]
    lea di, [dst_cmd]
    call copy_one
    jc fail
.files_ready:
    call verify_target_files
    jc fail
    mov ah, 0x0D
    int 0x21

    call parse_target_bpb
    jc fail
    call find_target_kernel
    jc fail
    call check_kernel_contiguous
    jc fail
    call build_rfat

    /* Preserve target BPB bytes 11..61, replace jump/OEM and boot code. */
    push ds
    pop es
    lea si, [boot_src]
    lea di, [boot_dst]
    mov cx, 11
    rep movsb
    lea si, [boot_src + 62]
    lea di, [boot_dst + 62]
    mov cx, 225
    rep movsw

    mov al, [target_drive]
    mov cx, 1
    mov dx, 1
    lea bx, [rfat]
    call abs_write
    jc fail
    mov al, [target_drive]
    mov cx, 1
    xor dx, dx
    lea bx, [boot_dst]
    call abs_write
    jc fail

done:
    lea dx, [msg_ok]
    mov ah, 9
    int 0x21
    mov ax, 0x4C00
    int 0x21

usage:
    lea dx, [msg_u]
    mov ah, 9
    int 0x21
    mov ax, 0x4C01
    int 0x21

fail:
    call close_copy_handles
    lea dx, [msg_e]
    mov ah, 9
    int 0x21
    mov ax, 0x4C01
    int 0x21

/* Patch drive letters into absolute source/target paths. */
build_paths:
    push ax
    mov al, [source_drive]
    add al, 'A'
    mov [src_kern], al
    mov [src_cmd], al
    mov al, [target_drive]
    add al, 'A'
    mov [dst_kern], al
    mov [dst_cmd], al
    pop ax
    ret

/* SI=source path, DI=target path. */
copy_one:
    mov word ptr [src_h], 0xFFFF
    mov word ptr [dst_h], 0xFFFF
    mov dx, si
    mov ax, 0x3D00
    int 0x21
    jc .co_fail
    mov [src_h], ax
    mov dx, di
    xor cx, cx
    mov ah, 0x3C
    int 0x21
    jc .co_fail
    mov [dst_h], ax
.co_loop:
    mov bx, [src_h]
    mov cx, 512
    lea dx, [xfer]
    mov ah, 0x3F
    int 0x21
    jc .co_fail
    test ax, ax
    jz .co_ok
    mov [xfer_count], ax
    mov bx, [dst_h]
    mov cx, ax
    lea dx, [xfer]
    mov ah, 0x40
    int 0x21
    jc .co_fail
    cmp ax, [xfer_count]
    jne .co_fail
    jmp .co_loop
.co_ok:
    call close_copy_handles
    clc
    ret
.co_fail:
    call close_copy_handles
    stc
    ret

close_copy_handles:
    push ax
    push bx
    mov bx, [src_h]
    cmp bx, 0xFFFF
    je .cch_dst
    mov ah, 0x3E
    int 0x21
    mov word ptr [src_h], 0xFFFF
.cch_dst:
    mov bx, [dst_h]
    cmp bx, 0xFFFF
    je .cch_done
    mov ah, 0x3E
    int 0x21
    mov word ptr [dst_h], 0xFFFF
.cch_done:
    pop bx
    pop ax
    ret

verify_target_files:
    lea dx, [dst_kern]
    call verify_one
    jc .vtf_bad
    lea dx, [dst_cmd]
    call verify_one
    ret
.vtf_bad:
    stc
    ret

verify_one:
    mov ax, 0x3D00
    int 0x21
    jc .vo_bad
    mov bx, ax
    mov ah, 0x3E
    int 0x21
    clc
    ret
.vo_bad:
    stc
    ret

/*
 * Parse the target BPB and derive FAT/root/data locations and FAT type.
 * boot_dst is the unmodified target sector.
 */
parse_target_bpb:
    mov al, [boot_dst + 13]
    test al, al
    jz .ptb_bad
    mov [bpb_spc], al
    mov ax, [boot_dst + 14]
    cmp ax, 2
    jb .ptb_bad
    mov [bpb_reserved], ax
    mov al, [boot_dst + 16]
    test al, al
    jz .ptb_bad
    mov [bpb_fats], al
    mov ax, [boot_dst + 17]
    mov [bpb_root_ents], ax
    add ax, 15
    mov cl, 4
    shr ax, cl
    mov [bpb_root_secs], ax
    mov ax, [boot_dst + 22]
    test ax, ax
    jz .ptb_bad
    mov [bpb_spf], ax

    xor ax, ax
    mov al, [bpb_fats]
    mul word ptr [bpb_spf]
    test dx, dx
    jnz .ptb_bad
    add ax, [bpb_reserved]
    jc .ptb_bad
    mov [bpb_root_lba], ax
    add ax, [bpb_root_secs]
    jc .ptb_bad
    mov [bpb_data_lba], ax

    mov ax, [boot_dst + 19]
    xor dx, dx
    test ax, ax
    jnz .ptb_total
    mov ax, [boot_dst + 32]
    mov dx, [boot_dst + 34]
.ptb_total:
    sub ax, [bpb_data_lba]
    sbb dx, 0
    jc .ptb_bad
    xor bx, bx
    mov bl, [bpb_spc]
    div bx
    cmp ax, 4085
    jb .ptb_fat12
    mov byte ptr [fat_type], 16
    clc
    ret
.ptb_fat12:
    mov byte ptr [fat_type], 12
    clc
    ret
.ptb_bad:
    stc
    ret

/* Locate KERNEL.SYS in the target root and derive RFAT sector/cluster counts. */
find_target_kernel:
    mov ax, [bpb_root_lba]
    mov [bpb_scan_lba], ax
    mov ax, [bpb_root_secs]
    mov [bpb_scan_left], ax
.ftk_sector:
    cmp word ptr [bpb_scan_left], 0
    jz .ftk_bad
    mov dx, [bpb_scan_lba]
    mov al, [target_drive]
    mov cx, 1
    lea bx, [diskbuf]
    call abs_read
    jc .ftk_bad

    lea di, [diskbuf]
    mov bx, 16
.ftk_entry:
    cmp byte ptr [di], 0
    je .ftk_bad
    cmp byte ptr [di], 0xE5
    je .ftk_next
    push bx
    push di
    lea si, [nm_kern]
    mov cx, 11
    repe cmpsb
    pop di
    pop bx
    je .ftk_found
.ftk_next:
    add di, 32
    dec bx
    jnz .ftk_entry
    inc word ptr [bpb_scan_lba]
    dec word ptr [bpb_scan_left]
    jmp .ftk_sector

.ftk_found:
    mov ax, [di + 26]
    cmp ax, 2
    jb .ftk_bad
    mov [kern_cluster], ax
    mov ax, [di + 30]
    test ax, ax
    jnz .ftk_bad
    mov ax, [di + 28]
    test ax, ax
    jz .ftk_bad
    add ax, 511
    jc .ftk_bad
    mov cl, 9
    shr ax, cl
    mov [kern_sectors], ax

    xor bx, bx
    mov bl, [bpb_spc]
    mov ax, [kern_sectors]
    add ax, bx
    dec ax
    xor dx, dx
    div bx
    mov [kern_clusters], ax

    mov ax, [kern_cluster]
    sub ax, 2
    mul bx
    test dx, dx
    jnz .ftk_bad
    add ax, [bpb_data_lba]
    jc .ftk_bad
    mov [kern_lba], ax
    clc
    ret
.ftk_bad:
    stc
    ret

/* The RFAT loader reads linearly, so reject a fragmented target KERNEL.SYS. */
check_kernel_contiguous:
    mov cx, [kern_clusters]
    cmp cx, 1
    jbe .ckc_ok
    mov ax, [kern_cluster]
.ckc_loop:
    mov bx, ax
    inc bx
    call fat_next
    jc .ckc_bad
    cmp ax, bx
    jne .ckc_bad
    dec cx
    cmp cx, 1
    jbe .ckc_ok
    mov ax, bx
    jmp .ckc_loop
.ckc_ok:
    clc
    ret
.ckc_bad:
    stc
    ret

/* AX=cluster -> AX=next cluster from target FAT1. */
fat_next:
    push bx
    push cx
    push dx
    push si
    mov si, ax
    cmp byte ptr [fat_type], 16
    je .fn_16_off
    mov bx, ax
    shr bx, 1
    add ax, bx
    jmp .fn_have_off
.fn_16_off:
    shl ax, 1
.fn_have_off:
    xor dx, dx
    mov bx, 512
    div bx
    mov [fat_off], dx
    add ax, [bpb_reserved]
    mov dx, ax
    mov al, [target_drive]
    mov cx, 1
    cmp byte ptr [fat_type], 16
    je .fn_read
    mov cx, 2
.fn_read:
    lea bx, [fatbuf]
    call abs_read
    jc .fn_bad
    mov bx, [fat_off]
    mov ax, [fatbuf + bx]
    cmp byte ptr [fat_type], 16
    je .fn_ok
    test si, 1
    jz .fn_12_even
    mov cl, 4
    shr ax, cl
    jmp .fn_12_mask
.fn_12_even:
.fn_12_mask:
    and ax, 0x0FFF
.fn_ok:
    pop si
    pop dx
    pop cx
    pop bx
    clc
    ret
.fn_bad:
    pop si
    pop dx
    pop cx
    pop bx
    stc
    ret

build_rfat:
    push ax
    push cx
    push di
    push es
    push ds
    pop es
    lea di, [rfat]
    mov cx, 256
    xor ax, ax
    rep stosw
    mov byte ptr [rfat], 'R'
    mov byte ptr [rfat + 1], 'F'
    mov byte ptr [rfat + 2], 'A'
    mov byte ptr [rfat + 3], 'T'
    mov byte ptr [rfat + 4], '1'
    mov byte ptr [rfat + 5], 1
    mov ax, [kern_lba]
    mov [rfat + 0x1C], ax
    mov ax, [kern_sectors]
    mov [rfat + 0x1E], ax
    pop es
    pop di
    pop cx
    pop ax
    ret

/* INT 25h/26h leave the caller FLAGS word on stack; discard it via BP. */
abs_read:
    push bp
    int 0x25
    pop bp
    pop bp
    ret

abs_write:
    push bp
    int 0x26
    pop bp
    pop bp
    ret

source_drive:
    .byte 0
target_drive:
    .byte 0
src_h:
    .word 0xFFFF
dst_h:
    .word 0xFFFF
xfer_count:
    .word 0
bpb_spc:
    .byte 1
bpb_fats:
    .byte 2
fat_type:
    .byte 12
bpb_reserved:
    .word 2
bpb_root_ents:
    .word 112
bpb_root_secs:
    .word 7
bpb_spf:
    .word 3
bpb_root_lba:
    .word 8
bpb_data_lba:
    .word 15
bpb_scan_lba:
    .word 0
bpb_scan_left:
    .word 0
kern_cluster:
    .word 0
kern_clusters:
    .word 0
kern_sectors:
    .word 0
kern_lba:
    .word 0
fat_off:
    .word 0
nm_kern:
    .ascii "KERNEL  SYS"
src_kern:
    .asciz "A:\\KERNEL.SYS"
src_cmd:
    .asciz "A:\\COMMAND.COM"
dst_kern:
    .asciz "A:\\KERNEL.SYS"
dst_cmd:
    .asciz "A:\\COMMAND.COM"
msg_ok:
    .ascii "System transferred\r\nSYS OK\r\n$"
msg_u:
    .ascii "SYS [d:]\r\n$"
msg_e:
    .ascii "SYS failed\r\n$"
xfer:
    .space 512, 0
boot_src:
    .space 512, 0
boot_dst:
    .space 512, 0
rfat:
    .space 512, 0
diskbuf:
    .space 512, 0
fatbuf:
    .space 1024, 0
