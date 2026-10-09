/*
 * LEASE.DAT file I/O. Requires netlease_defs.inc first.
 * Caller data: lease_path (ASCIZ), lease_handle, lease_buf[LEASE_SIZE].
 * lease_read_file: open/read/validate into lease_buf (CF=fail).
 * lease_write_file: create/write lease_buf (CF=fail).
 */

lease_read_file:
    push ax
    push bx
    push cx
    push dx
    mov ah, 0x3D
    xor al, al
    lea dx, [lease_path]
    int 0x21
    jc .lr_fail
    mov [lease_handle], ax

    mov ah, 0x3F
    mov bx, [lease_handle]
    mov cx, LEASE_SIZE
    lea dx, [lease_buf]
    int 0x21
    jc .lr_close_fail
    cmp ax, LEASE_SIZE
    jne .lr_close_fail

    mov ah, 0x3E
    mov bx, [lease_handle]
    int 0x21

    cmp word ptr [lease_buf], 0x4844     /* 'DH' */
    jne .lr_fail
    cmp word ptr [lease_buf + 2], 0x5043 /* 'CP' */
    jne .lr_fail
    cmp byte ptr [lease_buf + LEASE_OFF_VER], LEASE_VER
    jne .lr_fail
    clc
    jmp .lr_done

.lr_close_fail:
    mov ah, 0x3E
    mov bx, [lease_handle]
    int 0x21
.lr_fail:
    stc
.lr_done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

lease_write_file:
    push ax
    push bx
    push cx
    push dx
    mov ah, 0x3C
    xor cx, cx
    lea dx, [lease_path]
    int 0x21
    jc .lw_fail
    mov [lease_handle], ax

    mov ah, 0x40
    mov bx, [lease_handle]
    mov cx, LEASE_SIZE
    lea dx, [lease_buf]
    int 0x21
    jc .lw_close_fail
    cmp ax, LEASE_SIZE
    jne .lw_close_fail

    mov ah, 0x3E
    mov bx, [lease_handle]
    int 0x21
    clc
    jmp .lw_done

.lw_close_fail:
    mov ah, 0x3E
    mov bx, [lease_handle]
    int 0x21
.lw_fail:
    stc
.lw_done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret
