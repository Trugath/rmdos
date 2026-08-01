/*
 * DEFLATE inflate (RFC 1951) for gzip gunzip.
 *
 * Requires (defined by caller before include, or here):
 *   in_handle, out_handle  — DOS handles
 *   win_seg                — allocated 32 KiB window segment
 *
 * Entry: inflate_stream  CF=0 ok, CF=1 error (inf_err set)
 * Uses crc32_* from crc32.inc (update each output byte before final).
 * Tracks isize_lo/isize_hi (uncompressed byte count mod 2^32).
 */

.set INF_MAXBITS, 15
.set INF_MAXLCODES, 286
.set INF_MAXDCODES, 30
.set INF_MAXCODES, 316
.set INF_FIXLCODES, 288

/* ---- bit input ---- */
inf_bits:
    /* CX = n bits (1..15) → AX = value, CF on error */
    push bx
    push dx
    push si
    mov si, cx
    xor ax, ax
    mov dx, 1
.inf_bits_loop:
    cmp word ptr [inf_bitcnt], 0
    jne .inf_have
    call inf_pull_byte
    jc .inf_bits_fail
    mov byte ptr [inf_bitbuf], al
    mov word ptr [inf_bitcnt], 8
.inf_have:
    mov bl, byte ptr [inf_bitbuf]
    shr byte ptr [inf_bitbuf], 1
    dec word ptr [inf_bitcnt]
    test bl, 1
    jz .inf_nobit
    or ax, dx
.inf_nobit:
    shl dx, 1
    dec si
    jnz .inf_bits_loop
    clc
    pop si
    pop dx
    pop bx
    ret
.inf_bits_fail:
    stc
    pop si
    pop dx
    pop bx
    ret

inf_pull_byte:
    /* → AL, CF=err/eof */
    push bx
    mov bx, word ptr [inf_inpos]
    cmp bx, word ptr [inf_inlen]
    jb .inf_from_buf
    call inf_refill
    jc .inf_pull_fail
    xor bx, bx
.inf_from_buf:
    mov al, byte ptr [inf_inbuf + bx]
    inc bx
    mov word ptr [inf_inpos], bx
    pop bx
    clc
    ret
.inf_pull_fail:
    pop bx
    stc
    ret

inf_refill:
    push ax
    push bx
    push cx
    push dx
    mov ah, 0x3F
    mov bx, word ptr [in_handle]
    mov cx, 256
    lea dx, [inf_inbuf]
    int 0x21
    jc .inf_refill_err
    test ax, ax
    jz .inf_refill_err
    mov word ptr [inf_inlen], ax
    mov word ptr [inf_inpos], 0
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.inf_refill_err:
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

/* Align bit reader to next byte boundary */
inf_byte_align:
    mov word ptr [inf_bitcnt], 0
    ret

/* ---- output + window ---- */
inf_put:
    /* AL = byte; CF on write error */
    push ax
    push bx
    push cx
    push dx
    push es
    push di
    call crc32_update
    add word ptr [isize_lo], 1
    adc word ptr [isize_hi], 0
    /* window */
    mov es, word ptr [win_seg]
    mov di, word ptr [win_pos]
    stosb
    and di, 0x7FFF
    mov word ptr [win_pos], di
    /* out buffer */
    mov bx, word ptr [inf_outlen]
    mov byte ptr [inf_outbuf + bx], al
    inc bx
    mov word ptr [inf_outlen], bx
    cmp bx, 256
    jb .inf_put_ok
    call inf_flush
    jc .inf_put_fail
.inf_put_ok:
    pop di
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.inf_put_fail:
    pop di
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

inf_flush:
    push ax
    push bx
    push cx
    push dx
    mov cx, word ptr [inf_outlen]
    test cx, cx
    jz .inf_flush_done
    mov ah, 0x40
    mov bx, word ptr [out_handle]
    lea dx, [inf_outbuf]
    int 0x21
    jc .inf_flush_err
    cmp ax, cx
    jne .inf_flush_err
.inf_flush_done:
    mov word ptr [inf_outlen], 0
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    ret
.inf_flush_err:
    pop dx
    pop cx
    pop bx
    pop ax
    stc
    ret

/* Copy match: CX=len, DX=dist */
inf_copy:
    push ax
    push bx
    push si
    push di
    push es
    mov es, word ptr [win_seg]
.inf_copy_loop:
    test cx, cx
    jz .inf_copy_done
    mov bx, word ptr [win_pos]
    sub bx, dx
    and bx, 0x7FFF
    mov al, es:[bx]
    call inf_put
    jc .inf_copy_fail
    dec cx
    jmp .inf_copy_loop
.inf_copy_done:
    pop es
    pop di
    pop si
    pop bx
    pop ax
    clc
    ret
.inf_copy_fail:
    pop es
    pop di
    pop si
    pop bx
    pop ax
    stc
    ret

/* ---- Huffman construct / decode (puff-style) ---- */
/*
 * BX = huff base (16 count words + symbol words)
 * SI = length bytes, [inf_nsym] = n
 */
inf_construct2:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push ds
    pop es
    cld
    mov word ptr [inf_huff], bx
    /* zero 16 counts */
    mov di, bx
    mov cx, 16
    xor ax, ax
    rep stosw
    mov cx, word ptr [inf_nsym]
    xor di, di
.inf_c2_cnt:
    cmp di, cx
    jae .inf_c2_check
    mov bx, si
    add bx, di
    mov al, [bx]
    xor ah, ah
    cmp ax, 15
    ja .inf_c2_fail
    add ax, ax
    mov bx, word ptr [inf_huff]
    add bx, ax
    inc word ptr [bx]
    inc di
    jmp .inf_c2_cnt
.inf_c2_check:
    mov bx, word ptr [inf_huff]
    cmp word ptr [bx], cx
    je .inf_c2_ok
    mov ax, 1                       /* left */
    mov di, 1                       /* len */
.inf_c2_left:
    cmp di, 15
    ja .inf_c2_offs
    shl ax, 1
    mov bx, word ptr [inf_huff]
    add bx, di
    add bx, di
    sub ax, word ptr [bx]
    js .inf_c2_fail
    inc di
    jmp .inf_c2_left
.inf_c2_offs:
    /* offs[1]=0; offs[len+1]=offs[len]+count[len] — store in inf_offs */
    mov word ptr [inf_offs + 2], 0  /* offs[1] */
    mov di, 1
.inf_c2_offloop:
    cmp di, 15
    jae .inf_c2_sym
    mov bx, word ptr [inf_huff]
    add bx, di
    add bx, di
    mov ax, word ptr [bx]           /* count[len] */
    mov bx, di
    add bx, bx
    mov dx, word ptr [inf_offs + bx] /* offs[len] */
    add dx, ax
    add bx, 2
    mov word ptr [inf_offs + bx], dx /* offs[len+1] */
    inc di
    jmp .inf_c2_offloop
.inf_c2_sym:
    /* for symbol in 0..n-1: if length[s]: symbol[offs[len]++] = s */
    mov cx, word ptr [inf_nsym]
    xor di, di
.inf_c2_symloop:
    cmp di, cx
    jae .inf_c2_ok
    mov bx, si
    add bx, di
    mov al, [bx]
    test al, al
    jz .inf_c2_symnext
    xor ah, ah
    mov bx, ax
    add bx, bx
    mov dx, word ptr [inf_offs + bx] /* offs[len] */
    mov ax, dx
    inc dx
    mov word ptr [inf_offs + bx], dx
    /* store symbol at huff+32 + ax*2 */
    add ax, ax
    mov bx, word ptr [inf_huff]
    add bx, 32
    add bx, ax
    mov word ptr [bx], di
.inf_c2_symnext:
    inc di
    jmp .inf_c2_symloop
.inf_c2_ok:
    clc
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret
.inf_c2_fail:
    stc
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* Decode one symbol. BX = huff base. → AX = symbol, CF=error */
inf_decode:
    push bx
    push cx
    push dx
    mov word ptr [inf_huff], bx
    mov word ptr [inf_d_code], 0
    mov word ptr [inf_d_first], 0
    mov word ptr [inf_d_index], 0
    mov word ptr [inf_d_len], 1
.inf_dec_loop:
    cmp word ptr [inf_d_len], 15
    ja .inf_dec_fail
    mov cx, 1
    call inf_bits
    jc .inf_dec_fail
    or word ptr [inf_d_code], ax
    /* count = h->count[len] */
    mov bx, word ptr [inf_huff]
    mov cx, word ptr [inf_d_len]
    add bx, cx
    add bx, cx
    mov ax, word ptr [bx]           /* count */
    /* hit if code < first + count (unsigned) */
    mov dx, word ptr [inf_d_first]
    add dx, ax
    cmp word ptr [inf_d_code], dx
    jb .inf_dec_hit
    add word ptr [inf_d_index], ax
    mov bx, word ptr [inf_d_first]
    add bx, ax
    shl bx, 1
    mov word ptr [inf_d_first], bx
    shl word ptr [inf_d_code], 1
    inc word ptr [inf_d_len]
    jmp .inf_dec_loop
.inf_dec_hit:
    mov bx, word ptr [inf_d_code]
    sub bx, word ptr [inf_d_first]
    add bx, word ptr [inf_d_index]
    add bx, bx
    add bx, word ptr [inf_huff]
    add bx, 32
    mov ax, word ptr [bx]
    clc
    jmp .inf_dec_ret
.inf_dec_fail:
    stc
.inf_dec_ret:
    pop dx
    pop cx
    pop bx
    ret

/* Build fixed Huffman tables into inf_lencode / inf_distcode */
inf_fixed:
    push ax
    push cx
    push si
    push di
    push es
    push ds
    pop es
    cld
    /* copy precomputed lit counts + symbols (16 + 288 words) */
    lea si, [fixed_lit_tab]
    lea di, [inf_lencode]
    mov cx, 16+288
    rep movsw
    /* dist: 16 counts + 30 symbols */
    lea si, [fixed_dist_tab]
    lea di, [inf_distcode]
    mov cx, 16+30
    rep movsw
    pop es
    pop di
    pop si
    pop cx
    pop ax
    clc
    ret

/* counts[16] then symbols[288] */
fixed_lit_tab:
    .word 0,0,0,0,0,0,0,24,152,112,0,0,0,0,0,0
    .word 256,257,258,259,260,261,262,263,264,265,266,267,268,269,270,271
    .word 272,273,274,275,276,277,278,279,0,1,2,3,4,5,6,7
    .word 8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23
    .word 24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39
    .word 40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55
    .word 56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71
    .word 72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87
    .word 88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103
    .word 104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119
    .word 120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135
    .word 136,137,138,139,140,141,142,143,280,281,282,283,284,285,286,287
    .word 144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159
    .word 160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175
    .word 176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191
    .word 192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207
    .word 208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223
    .word 224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239
    .word 240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255

fixed_dist_tab:
    .word 0,0,0,0,0,30,0,0,0,0,0,0,0,0,0,0
    .word 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29

/* ---- blocks ---- */
inf_stored:
    call inf_byte_align
    /* LEN and NLEN as raw little-endian bytes (byte-aligned) */
    call inf_pull_byte
    jc .inf_st_fail
    mov byte ptr [inf_sllen], al
    call inf_pull_byte
    jc .inf_st_fail
    mov byte ptr [inf_sllen+1], al
    call inf_pull_byte
    jc .inf_st_fail
    mov byte ptr [inf_nlen_tmp], al
    call inf_pull_byte
    jc .inf_st_fail
    mov byte ptr [inf_nlen_tmp+1], al
    mov ax, word ptr [inf_nlen_tmp]
    not ax
    cmp ax, word ptr [inf_sllen]
    je .inf_st_lenok
    mov byte ptr [inf_err], 'N'
    stc
    ret
.inf_st_lenok:
    mov cx, word ptr [inf_sllen]
.inf_st_loop:
    test cx, cx
    jz .inf_st_ok
    push cx
    call inf_pull_byte
    pop cx
    jc .inf_st_fail
    push cx
    call inf_put
    pop cx
    jc .inf_st_fail
    dec cx
    jmp .inf_st_loop
.inf_st_ok:
    clc
    ret
.inf_st_fail:
    stc
    ret

/* Decode lit/len and dist using current tables; until EOB */
inf_codes:
    /* sanity: fixed lit count[7] must be 24 */
    mov bx, word ptr [inf_lit_ptr]
    cmp word ptr [bx + 14], 24
    je .inf_codes_loop
    mov byte ptr [inf_err], 't'
    stc
    ret
.inf_codes_loop:
    mov bx, word ptr [inf_lit_ptr]
    call inf_decode
    jc .inf_codes_fail_dec
    cmp ax, 256
    je .inf_codes_ok
    jb .inf_codes_lit
    /* length */
    sub ax, 257
    cmp ax, 29
    jae .inf_codes_fail_len
    mov bx, ax
    add bx, bx
    mov cx, word ptr [inf_lenbits + bx]
    mov dx, word ptr [inf_lensym + bx]  /* base */
    push dx
    test cx, cx
    jz .inf_codes_nob
    call inf_bits
    jc .inf_codes_fail2
    pop dx
    add dx, ax
    push dx
    jmp .inf_codes_gotlen
.inf_codes_nob:
    xor ax, ax
.inf_codes_gotlen:
    pop word ptr [inf_copylen]
    /* distance */
    mov bx, word ptr [inf_dist_ptr]
    call inf_decode
    jc .inf_codes_fail_dec
    cmp ax, 30
    jae .inf_codes_fail_len
    mov bx, ax
    add bx, bx
    mov cx, word ptr [inf_distbits + bx]
    mov dx, word ptr [inf_distsym + bx]
    push dx
    test cx, cx
    jz .inf_codes_nod
    call inf_bits
    jc .inf_codes_fail2
    pop dx
    add dx, ax
    jmp .inf_codes_gotdist
.inf_codes_nod:
    pop dx
.inf_codes_gotdist:
    mov cx, word ptr [inf_copylen]
    call inf_copy
    jc .inf_codes_fail_put
    jmp .inf_codes_loop
.inf_codes_lit:
    call inf_put
    jc .inf_codes_fail_put
    jmp .inf_codes_loop
.inf_codes_ok:
    clc
    ret
.inf_codes_fail2:
    pop dx
.inf_codes_fail_dec:
    mov byte ptr [inf_err], 'd'
    stc
    ret
.inf_codes_fail_len:
    mov byte ptr [inf_err], 'L'
    stc
    ret
.inf_codes_fail_put:
    mov byte ptr [inf_err], 'P'
    stc
    ret
.inf_codes_fail:
    stc
    ret

inf_codes_fixed:
    jmp inf_codes

/* Dynamic Huffman block */
inf_dynamic:
    mov cx, 5
    call inf_bits
    jc .inf_dyn_fail
    add ax, 257
    mov word ptr [inf_nlit], ax
    mov cx, 5
    call inf_bits
    jc .inf_dyn_fail
    add ax, 1
    mov word ptr [inf_ndist], ax
    mov cx, 4
    call inf_bits
    jc .inf_dyn_fail
    add ax, 4
    mov word ptr [inf_nclen], ax
    /* zero first 19 lens */
    mov cx, 19
    lea di, [inf_lens]
    xor ax, ax
    rep stosb
    xor bx, bx
.inf_dyn_clen:
    cmp bx, word ptr [inf_nclen]
    jae .inf_dyn_buildc
    mov cx, 3
    call inf_bits
    jc .inf_dyn_fail
    mov si, bx
    mov dl, byte ptr [inf_clen_order + si]
    xor dh, dh
    mov si, dx
    mov byte ptr [inf_lens + si], al
    inc bx
    jmp .inf_dyn_clen
.inf_dyn_buildc:
    mov word ptr [inf_nsym], 19
    lea si, [inf_lens]
    lea bx, [inf_lencode]           /* reuse as code-length alphabet */
    call inf_construct2
    jc .inf_dyn_fail
    /* decode nlit+ndist code lengths into inf_lens */
    mov ax, word ptr [inf_nlit]
    add ax, word ptr [inf_ndist]
    mov word ptr [inf_need], ax
    xor bx, bx                      /* index into inf_lens */
.inf_dyn_ll:
    cmp bx, word ptr [inf_need]
    jae .inf_dyn_buildld
    push bx
    lea bx, [inf_lencode]
    call inf_decode
    pop bx
    jc .inf_dyn_fail
    cmp ax, 16
    jb .inf_dyn_direct
    cmp ax, 16
    je .inf_dyn_rep16
    cmp ax, 17
    je .inf_dyn_rep17
    /* 18: repeat 0 for 11+bits(7) */
    mov cx, 7
    call inf_bits
    jc .inf_dyn_fail
    add ax, 11
    mov cx, ax
    xor al, al
    jmp .inf_dyn_fill
.inf_dyn_rep17:
    mov cx, 3
    call inf_bits
    jc .inf_dyn_fail
    add ax, 3
    mov cx, ax
    xor al, al
    jmp .inf_dyn_fill
.inf_dyn_rep16:
    test bx, bx
    jz .inf_dyn_fail
    mov al, byte ptr [inf_lens + bx - 1]
    push ax
    mov cx, 2
    call inf_bits
    jc .inf_dyn_fail3
    add ax, 3
    mov cx, ax
    pop ax
.inf_dyn_fill:
.inf_dyn_fill_loop:
    test cx, cx
    jz .inf_dyn_ll
    cmp bx, word ptr [inf_need]
    jae .inf_dyn_fail
    mov byte ptr [inf_lens + bx], al
    inc bx
    dec cx
    jmp .inf_dyn_fill_loop
.inf_dyn_direct:
    mov byte ptr [inf_lens + bx], al
    inc bx
    jmp .inf_dyn_ll
.inf_dyn_fail3:
    pop ax
.inf_dyn_fail:
    stc
    ret
.inf_dyn_buildld:
    mov ax, word ptr [inf_nlit]
    mov word ptr [inf_nsym], ax
    lea si, [inf_lens]
    lea bx, [inf_lencode]
    call inf_construct2
    jc .inf_dyn_fail
    mov word ptr [inf_lit_ptr], bx
    mov ax, word ptr [inf_ndist]
    mov word ptr [inf_nsym], ax
    lea si, [inf_lens]
    add si, word ptr [inf_nlit]
    lea bx, [inf_distcode]
    call inf_construct2
    jc .inf_dyn_fail
    mov word ptr [inf_dist_ptr], bx
    call inf_codes
    ret

/* Main inflate: process blocks until BFINAL */
inflate_stream:
    mov word ptr [inf_bitcnt], 0
    mov word ptr [inf_inpos], 0
    mov word ptr [inf_inlen], 0
    mov word ptr [inf_outlen], 0
    mov word ptr [win_pos], 0
    mov word ptr [isize_lo], 0
    mov word ptr [isize_hi], 0
    call crc32_init
.inf_blk:
    mov cx, 1
    call inf_bits
    jc .inf_fail_bits
    mov word ptr [inf_bfinal], ax
    mov cx, 2
    call inf_bits
    jc .inf_fail_bits
    cmp ax, 0
    je .inf_do_stored
    cmp ax, 1
    je .inf_do_fixed
    cmp ax, 2
    je .inf_do_dyn
    mov byte ptr [inf_err], 'B'
    jmp .inf_fail
.inf_do_stored:
    call inf_stored
    jc .inf_fail_stored
    jmp .inf_blk_next
.inf_do_fixed:
    /* use ROM tables directly — no copy */
    lea bx, [fixed_lit_tab]
    mov word ptr [inf_lit_ptr], bx
    lea bx, [fixed_dist_tab]
    mov word ptr [inf_dist_ptr], bx
    call inf_codes_fixed
    jc .inf_fail_codes
    jmp .inf_blk_next
.inf_do_dyn:
    call inf_dynamic
    jc .inf_fail_dyn
.inf_blk_next:
    cmp word ptr [inf_bfinal], 0
    je .inf_blk
    call inf_flush
    jc .inf_fail_flush
    call crc32_final
    clc
    ret
.inf_fail_bits:
    mov byte ptr [inf_err], 'b'
    stc
    ret
.inf_fail_stored:
    /* keep more specific inf_err if already set */
    cmp byte ptr [inf_err], '?'
    jne .inf_fail_stored_ret
    mov byte ptr [inf_err], 'S'
.inf_fail_stored_ret:
    stc
    ret
.inf_fail_fixed:
    mov byte ptr [inf_err], 'F'
    stc
    ret
.inf_fail_codes:
    /* inf_err already set by inf_codes */
    stc
    ret
.inf_fail_dyn:
    mov byte ptr [inf_err], 'D'
    stc
    ret
.inf_fail_flush:
    mov byte ptr [inf_err], 'f'
    stc
    ret
.inf_fail:
    stc
    ret

/* ---- tables & state ---- */
inf_clen_order:
    .byte 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15

/* length base / extra bits for symbols 257..285 (index 0..28) */
inf_lensym:
    .word 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31
    .word 35,43,51,59,67,83,99,115,131,163,195,227,258
inf_lenbits:
    .word 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2
    .word 3,3,3,3,4,4,4,4,5,5,5,5,0

inf_distsym:
    .word 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193
    .word 257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
inf_distbits:
    .word 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6
    .word 7,7,8,8,9,9,10,10,11,11,12,12,13,13

inf_bitbuf: .byte 0
            .byte 0
inf_bitcnt: .word 0
inf_inpos:  .word 0
inf_inlen:  .word 0
inf_outlen: .word 0
inf_nsym:   .word 0
inf_huff:   .word 0
inf_bfinal: .word 0
inf_sllen:  .word 0
inf_nlen_tmp:.word 0
inf_copylen:.word 0
inf_nlit:   .word 0
inf_ndist:  .word 0
inf_nclen:  .word 0
inf_need:   .word 0
win_pos:    .word 0
isize_lo:   .word 0
isize_hi:   .word 0
inf_err:    .byte '?'
            .byte 0
inf_d_code: .word 0
inf_d_first:.word 0
inf_d_index:.word 0
inf_d_len:  .word 0
inf_lit_ptr:.word 0
inf_dist_ptr:.word 0
inf_dbg_len:.word 0
inf_dbg_cx: .word 0
inf_dbg_b0: .byte 0
inf_dbg_b1: .byte 0
inf_dbg_b2: .byte 0
            .byte 0

inf_offs:   .space 32, 0          /* 16 words */
inf_inbuf:  .space 256, 0
inf_outbuf: .space 256, 0
inf_lens:   .space 320, 0         /* code lengths scratch */

/* huffman: 16 count words + up to 288 symbol words */
inf_lencode: .space 608, 0
inf_distcode:.space 96, 0
