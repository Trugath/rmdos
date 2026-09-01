/*
 * DEFLATE compress for gzip: greedy LZ77 + fixed Huffman (BTYPE=01).
 * Requires: in_handle, out_handle
 * Entry: deflate_stream — CF=0 ok
 * Updates crc32_* and isize_* on input bytes.
 */

.equ DEF_BLK, 1024

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
    call def_fixed_stream
    jc .defs_fail
    call crc32_final
    clc
    ret
.defs_fail:
    stc
    ret

/* Stored blocks (BTYPE=00). Loop until EOF; BFINAL only on the last block. */
def_stored_stream:
    mov byte ptr [def_saw_eof], 0
.dss_block:
    mov word ptr [def_blklen], 0
.dss_read:
    cmp word ptr [def_blklen], 256
    jae .dss_emit
    call def_get
    jc .dss_fail
    jz .dss_eof
    mov bx, word ptr [def_blklen]
    mov byte ptr [def_blkbuf + bx], al
    inc bx
    mov word ptr [def_blklen], bx
    call crc32_update
    add word ptr [isize_lo], 1
    adc word ptr [isize_hi], 0
    jmp .dss_read
.dss_eof:
    mov byte ptr [def_saw_eof], 1
.dss_emit:
    xor ax, ax
    cmp byte ptr [def_saw_eof], 0
    je .dss_hdr
    mov ax, 1                    /* BFINAL */
.dss_hdr:
    mov cx, 3                    /* BFINAL + BTYPE=00 */
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
    jae .dss_block_done
    mov al, byte ptr [def_blkbuf + bx]
    push bx
    xor ah, ah
    mov cx, 8
    call def_putbits
    pop bx
    jc .dss_fail
    inc bx
    jmp .dss_out
.dss_block_done:
    cmp byte ptr [def_saw_eof], 0
    je .dss_block
    call def_align_byte
    jc .dss_fail
    call def_flush_buf
    clc
    ret
.dss_fail:
    stc
    ret

/* Reverse AX's low CX bits so Huffman MSB is emitted first (def_putbits is LSB-first). */
def_rev_put:
    push bx
    push dx
    push si
    mov si, cx
    mov dx, ax
    xor ax, ax
.drp:
    shl ax, 1
    shr dx, 1
    jnc .drp0
    or ax, 1
.drp0:
    dec si
    jnz .drp
    pop si
    call def_putbits
    pop dx
    pop bx
    ret

/* AX = literal/length symbol 0..287 */
def_emit_hlit:
    push bx
    push cx
    push dx
    cmp ax, 144
    jb .he8
    cmp ax, 256
    jb .he9
    cmp ax, 280
    jb .he7
    sub ax, 280
    add ax, 0xC0
    mov cx, 8
    jmp .he_out
.he7:
    sub ax, 256
    mov cx, 7
    jmp .he_out
.he9:
    sub ax, 144
    add ax, 0x190
    mov cx, 9
    jmp .he_out
.he8:
    add ax, 0x30
    mov cx, 8
.he_out:
    call def_rev_put
    pop dx
    pop cx
    pop bx
    ret

/* CX = match length 3..258, DX = distance 1..32768 */
def_emit_match:
    push ax
    push bx
    push cx
    push dx
    push si
    mov word ptr [def_mlen], cx
    mov word ptr [def_mdist], dx
    xor bx, bx
.dem_l:
    mov ax, bx
    inc ax
    cmp ax, 29
    jae .dem_luse
    mov si, ax
    add si, si
    mov ax, word ptr [def_lenbase + si]
    cmp word ptr [def_mlen], ax
    jb .dem_luse
    inc bx
    jmp .dem_l
.dem_luse:
    mov si, bx
    add si, si
    mov ax, bx
    add ax, 257
    call def_emit_hlit
    jc .dem_fail
    mov cx, word ptr [def_lenbits + si]
    test cx, cx
    jz .dem_d
    mov ax, word ptr [def_mlen]
    sub ax, word ptr [def_lenbase + si]
    call def_putbits
    jc .dem_fail
.dem_d:
    xor bx, bx
.dem_di:
    mov ax, bx
    inc ax
    cmp ax, 30
    jae .dem_duse
    mov si, ax
    add si, si
    mov ax, word ptr [def_distbase + si]
    cmp word ptr [def_mdist], ax
    jb .dem_duse
    inc bx
    jmp .dem_di
.dem_duse:
    mov si, bx
    add si, si
    mov ax, bx
    mov cx, 5
    call def_rev_put
    jc .dem_fail
    mov cx, word ptr [def_distbits + si]
    test cx, cx
    jz .dem_ok
    mov ax, word ptr [def_mdist]
    sub ax, word ptr [def_distbase + si]
    call def_putbits
    jc .dem_fail
.dem_ok:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.dem_fail:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

/* Greedy LZ77 + one fixed-Huffman block per chunk (up to DEF_BLK). */
def_fixed_stream:
    mov byte ptr [def_saw_eof], 0
.dfs_block:
    mov word ptr [def_blklen], 0
.dfs_read:
    cmp word ptr [def_blklen], DEF_BLK
    jae .dfs_go
    call def_get
    jc .dfs_fail
    jz .dfs_eof
    mov bx, word ptr [def_blklen]
    mov byte ptr [def_blkbuf + bx], al
    inc bx
    mov word ptr [def_blklen], bx
    call crc32_update
    add word ptr [isize_lo], 1
    adc word ptr [isize_hi], 0
    jmp .dfs_read
.dfs_eof:
    mov byte ptr [def_saw_eof], 1
.dfs_go:
    xor ax, ax
    cmp byte ptr [def_saw_eof], 0
    je .dfs_hdr
    or ax, 1                     /* BFINAL */
.dfs_hdr:
    or ax, 2                     /* BTYPE = 01 (fixed) */
    mov cx, 3
    call def_putbits
    jc .dfs_fail
    mov word ptr [def_pos], 0
.dfs_lz:
    mov ax, word ptr [def_pos]
    cmp ax, word ptr [def_blklen]
    jae .dfs_eob
    call def_find_match
    cmp ax, 3
    jb .dfs_lit
    mov cx, ax
    call def_emit_match
    jc .dfs_fail
    add word ptr [def_pos], cx
    jmp .dfs_lz
.dfs_lit:
    mov bx, word ptr [def_pos]
    mov al, byte ptr [def_blkbuf + bx]
    xor ah, ah
    call def_emit_hlit
    jc .dfs_fail
    inc word ptr [def_pos]
    jmp .dfs_lz
.dfs_eob:
    mov ax, 256
    call def_emit_hlit
    jc .dfs_fail
    cmp byte ptr [def_saw_eof], 0
    je .dfs_block
    call def_align_byte
    jc .dfs_fail
    call def_flush_buf
    clc
    ret
.dfs_fail:
    stc
    ret

/*
 * At def_pos, search earlier bytes in this chunk.
 * OUT: AX=best length (1 if none), DX=best distance.
 */
def_find_match:
    push bx
    push cx
    push si
    push di
    mov word ptr [def_bestlen], 1
    mov word ptr [def_bestdist], 0
    mov si, word ptr [def_pos]
    test si, si
    jz .dfm_done
    xor dx, dx                   /* dist */
.dfm_d:
    inc dx
    cmp dx, si
    ja .dfm_done
    cmp dx, 4096
    ja .dfm_done
    mov di, si
    sub di, dx
    xor bx, bx                   /* len */
.dfm_c:
    mov ax, si
    add ax, bx
    cmp ax, word ptr [def_blklen]
    jae .dfm_got
    cmp bx, 258
    jae .dfm_got
    mov di, si
    sub di, dx
    add di, bx
    mov al, byte ptr [def_blkbuf + di]
    mov di, si
    add di, bx
    cmp al, byte ptr [def_blkbuf + di]
    jne .dfm_got
    inc bx
    jmp .dfm_c
.dfm_got:
    cmp bx, word ptr [def_bestlen]
    jbe .dfm_d
    cmp bx, 3
    jb .dfm_d
    mov word ptr [def_bestlen], bx
    mov word ptr [def_bestdist], dx
    cmp bx, 258
    jae .dfm_done
    jmp .dfm_d
.dfm_done:
    mov ax, word ptr [def_bestlen]
    mov dx, word ptr [def_bestdist]
    pop di
    pop si
    pop cx
    pop bx
    ret

def_bitbuf:  .byte 0
             .byte 0
def_bitcnt:  .word 0
def_outlen:  .word 0
def_inpos:   .word 0
def_inlen:   .word 0
def_blklen:  .word 0
def_saw_eof: .byte 0
             .byte 0
    .align 2
def_pos:     .word 0
def_bestlen: .word 0
def_bestdist:.word 0
def_mlen:    .word 0
def_mdist:   .word 0
isize_lo:    .word 0
isize_hi:    .word 0

    .align 2
def_lenbase:
    .word 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31
    .word 35,43,51,59,67,83,99,115,131,163,195,227,258
def_lenbits:
    .word 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2
    .word 3,3,3,3,4,4,4,4,5,5,5,5,0
def_distbase:
    .word 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193
    .word 257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
def_distbits:
    .word 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6
    .word 7,7,8,8,9,9,10,10,11,11,12,12,13,13

def_outbuf:  .space 256, 0
def_inbuf:   .space 256, 0
def_blkbuf:  .space DEF_BLK, 0
