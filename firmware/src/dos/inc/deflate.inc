/*
 * DEFLATE compress for gzip: stored blocks (BTYPE=00).
 * Requires: in_handle, out_handle
 * Entry: deflate_stream — CF=0 ok
 * Updates crc32_* and isize_* on input bytes.
 */

def_putbits:
    push bx
    push dx
    push si
    mov si, cx
    test si, si
    jz .dpb_done
.dpb_loop:
    test ax, 1
    jz .dpb_zero
    mov bx, word ptr [def_bitcnt]
    mov dl, 1
    mov cl, bl
    shl dl, cl
    or byte ptr [def_bitbuf], dl
.dpb_zero:
    shr ax, 1
    inc word ptr [def_bitcnt]
    cmp word ptr [def_bitcnt], 8
    jb .dpb_next
    call def_flush_byte
    jc .dpb_fail
.dpb_next:
    dec si
    jnz .dpb_loop
.dpb_done:
    clc
    pop si
    pop dx
    pop bx
    ret
.dpb_fail:
    stc
    pop si
    pop dx
    pop bx
    ret

def_flush_byte:
    push ax
    push bx
    push cx
    push dx
    mov al, byte ptr [def_bitbuf]
    mov bx, word ptr [def_outlen]
    mov byte ptr [def_outbuf + bx], al
    inc bx
    mov word ptr [def_outlen], bx
    mov byte ptr [def_bitbuf], 0
    mov word ptr [def_bitcnt], 0
    cmp bx, 256
    jb .dfb_ok
    call def_flush_buf
    jc .dfb_fail
.dfb_ok:
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.dfb_fail:
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

def_flush_buf:
    push ax
    push bx
    push cx
    push dx
    mov cx, word ptr [def_outlen]
    test cx, cx
    jz .dfbuf_done
    mov ah, 0x40
    mov bx, word ptr [out_handle]
    lea dx, [def_outbuf]
    int 0x21
    jc .dfbuf_err
    cmp ax, cx
    jne .dfbuf_err
.dfbuf_done:
    mov word ptr [def_outlen], 0
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.dfbuf_err:
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

def_align_byte:
    cmp word ptr [def_bitcnt], 0
    je .dab_ok
    call def_flush_byte
.dab_ok:
    ret

def_get:
    push bx
    push cx
    push dx
    mov bx, word ptr [def_inpos]
    cmp bx, word ptr [def_inlen]
    jb .dg_have
    mov ah, 0x3F
    mov bx, word ptr [in_handle]
    mov cx, 256
    lea dx, [def_inbuf]
    int 0x21
    jc .dg_err
    test ax, ax
    jz .dg_eof
    mov word ptr [def_inlen], ax
    xor bx, bx
    mov word ptr [def_inpos], 0
.dg_have:
    mov al, byte ptr [def_inbuf + bx]
    inc bx
    mov word ptr [def_inpos], bx
    pop dx
    pop cx
    pop bx
    mov ah, 1
    or ah, ah
    clc
    ret
.dg_eof:
    pop dx
    pop cx
    pop bx
    xor al, al
    cmp al, al
    clc
    ret
.dg_err:
    pop dx
    pop cx
    pop bx
    stc
    ret

deflate_stream:
    mov word ptr [def_bitcnt], 0
    mov byte ptr [def_bitbuf], 0
    mov word ptr [def_outlen], 0
    mov word ptr [def_inpos], 0
    mov word ptr [def_inlen], 0
    mov word ptr [isize_lo], 0
    mov word ptr [isize_hi], 0
    call crc32_init
    call def_stored_stream
    jc .defs_fail
    call crc32_final
    clc
    ret
.defs_fail:
    stc
    ret

/* One final stored block; input must fit in 256 bytes (guest tools use small files). */
def_stored_stream:
    mov word ptr [def_blklen], 0
.dss_read:
    cmp word ptr [def_blklen], 256
    jae .dss_emit
    call def_get
    jc .dss_fail
    jz .dss_emit
    mov bx, word ptr [def_blklen]
    mov byte ptr [def_blkbuf + bx], al
    inc bx
    mov word ptr [def_blklen], bx
    call crc32_update
    add word ptr [isize_lo], 1
    adc word ptr [isize_hi], 0
    jmp .dss_read
.dss_emit:
    mov ax, 1
    mov cx, 3
    call def_putbits
    jc .dss_fail
    call def_align_byte
    jc .dss_fail
    mov ax, word ptr [def_blklen]
    mov cx, 16
    call def_putbits
    jc .dss_fail
    mov ax, word ptr [def_blklen]
    not ax
    mov cx, 16
    call def_putbits
    jc .dss_fail
    xor bx, bx
.dss_out:
    cmp bx, word ptr [def_blklen]
    jae .dss_done
    mov al, byte ptr [def_blkbuf + bx]
    push bx
    xor ah, ah
    mov cx, 8
    call def_putbits
    pop bx
    jc .dss_fail
    inc bx
    jmp .dss_out
.dss_done:
    call def_align_byte
    jc .dss_fail
    call def_flush_buf
    clc
    ret
.dss_fail:
    stc
    ret

def_bitbuf:  .byte 0
             .byte 0
def_bitcnt:  .word 0
def_outlen:  .word 0
def_inpos:   .word 0
def_inlen:   .word 0
def_blklen:  .word 0
isize_lo:    .word 0
isize_hi:    .word 0

def_outbuf:  .space 256, 0
def_inbuf:   .space 256, 0
def_blkbuf:  .space 256, 0
