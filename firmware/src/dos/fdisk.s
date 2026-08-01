.code16
.intel_syntax noprefix
.section .text
.global _start

/* FDISK /P | /AUTO — one active primary DOS partition, <= 40 MiB. */
_start:
    push cs
    pop ds
    push cs
    pop es
    mov byte ptr [drive], 0x80
    mov si, 0x81
    call parse_args
    jc usage
    call geometry
    jc fail
    call write_mbr
    jc fail
    call write_vbr_template
    jc fail
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
    /* Accept /P, /AUTO, and an optional C: token. */
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
    je .switch_ok
    cmp al, 'A'
    jne .bad
    cmp byte ptr [si + 1], 'U'
    jne .bad
    cmp byte ptr [si + 2], 'T'
    jne .bad
    cmp byte ptr [si + 3], 'O'
    jne .bad
    add si, 4
    jmp .skip
.switch_ok:
    inc si
    jmp .skip
.ok:
    clc
    ret
.bad:
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
    mul word ptr [spt]           /* DX:AX total sectors */
    cmp dx, 1
    ja .geo_bad
    cmp dx, 1
    jb .size_ok
    cmp ax, 0x4000
    ja .geo_bad
.size_ok:
    cmp ax, 18                   /* reserve track zero, need a volume */
    jbe .geo_bad
    mov [total], ax
    sub ax, 17                   /* CHS 0/1/1 */
    mov [part_secs], ax
    clc
    ret
.geo_bad:
    stc
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

/* AX=physical LBA, ES:BX=buffer. */
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

write_mbr:
    call clear_buf
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
    lea di, [secbuf + 0x1BE]
    mov byte ptr [di], 0x80
    mov byte ptr [di + 1], 1      /* start CHS: C0/H1/S1 */
    mov byte ptr [di + 2], 1
    mov byte ptr [di + 3], 0
    mov al, [part_secs + 1]
    test al, al
    jnz .fat16_large
    cmp word ptr [part_secs], 32768
    jae .fat16_small
    mov byte ptr [di + 4], 0x01   /* FAT12 */
    jmp .type_done
.fat16_small:
    mov byte ptr [di + 4], 0x04
    jmp .type_done
.fat16_large:
    mov byte ptr [di + 4], 0x06
.type_done:
    mov byte ptr [di + 5], 0xFF   /* end CHS is advisory */
    mov byte ptr [di + 6], 0xFF
    mov byte ptr [di + 7], 0xFF
    mov word ptr [di + 8], 17
    mov word ptr [di + 10], 0
    mov ax, [part_secs]
    mov [di + 12], ax
    mov word ptr [di + 14], 0
    mov word ptr [secbuf + 510], 0xAA55
    xor ax, ax
    lea bx, [secbuf]
    call write_lba
    ret

/*
 * Seed the partition with an rmDOS VBR loader. FORMAT preserves this code and
 * supplies the final BPB, RFAT1, and system files.
 */
write_vbr_template:
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
    mov ax, 17
    lea bx, [secbuf]
    call write_lba
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
spt:        .word 17
heads:      .word 4
total:      .word 0
part_secs:  .word 0
secbuf:     .space 512, 0
msg_ok:     .ascii "FDISK OK\r\n$"
msg_usage:  .ascii "FDISK [/P|/AUTO] [C:]\r\n$"
msg_fail:   .ascii "FDISK failed\r\n$"
