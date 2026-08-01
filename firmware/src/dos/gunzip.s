.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * GUNZIP.COM — decompress a gzip member.
 * Usage: GUNZIP [src [dst]]
 *   0 args: stdin → stdout
 *   1 arg:  src → stdout
 *   2 args: src → dst
 * Status text is omitted when writing to stdout (pipe/redirect safe).
 */

_start:
    push cs
    pop ds
    push cs
    pop es

    mov word ptr [in_handle], 0
    mov word ptr [out_handle], 1
    mov byte ptr [in_is_file], 0
    mov byte ptr [out_is_file], 0
    mov word ptr [win_seg], 0

    call parse_args

    cmp byte ptr [have_src], 0
    je .have_in
    mov ah, 0x3D
    xor al, al
    lea dx, [src_path]
    int 0x21
    jc fail_open
    mov word ptr [in_handle], ax
    mov byte ptr [in_is_file], 1
.have_in:
    cmp byte ptr [have_dst], 0
    je .have_out
    mov ah, 0x3C
    xor cx, cx
    lea dx, [dst_path]
    int 0x21
    jc fail_create
    mov word ptr [out_handle], ax
    mov byte ptr [out_is_file], 1
.have_out:
    /* allocate 32 KiB window */
    mov ah, 0x48
    mov bx, 0x0800
    int 0x21
    jc fail_nomem
    mov word ptr [win_seg], ax

    call gunzip_member
    jc fail_data

    call free_window
    call close_handles

    cmp byte ptr [out_is_file], 0
    je .exit_ok
    mov ah, 0x09
    lea dx, [msg_ok]
    int 0x21
.exit_ok:
    mov ax, 0x4C00
    int 0x21

fail_open:
    mov ah, 0x09
    lea dx, [msg_e]
    int 0x21
    mov ax, 0x4C01
    int 0x21

fail_create:
    call close_handles
    jmp fail_open

fail_nomem:
    call close_handles
    mov ah, 0x09
    lea dx, [msg_mem]
    int 0x21
    mov ax, 0x4C01
    int 0x21

fail_data:
    call free_window
    call close_handles
    mov ah, 0x09
    lea dx, [msg_e]
    int 0x21
    mov ax, 0x4C01
    int 0x21

free_window:
    cmp word ptr [win_seg], 0
    je .fw_done
    mov es, word ptr [win_seg]
    mov ah, 0x49
    int 0x21
    push cs
    pop es
    mov word ptr [win_seg], 0
.fw_done:
    ret

close_handles:
    cmp byte ptr [out_is_file], 0
    je .ch_in
    mov ah, 0x3E
    mov bx, word ptr [out_handle]
    int 0x21
.ch_in:
    cmp byte ptr [in_is_file], 0
    je .ch_done
    mov ah, 0x3E
    mov bx, word ptr [in_handle]
    int 0x21
.ch_done:
    ret

parse_args:
    mov byte ptr [have_src], 0
    mov byte ptr [have_dst], 0
    mov si, 0x81
    call skip_ws
    jc .pa_ok
    lea di, [src_path]
    call copy_tok
    jc .pa_ok
    mov byte ptr [have_src], 1
    call skip_ws
    jc .pa_ok
    lea di, [dst_path]
    call copy_tok
    jc .pa_ok
    mov byte ptr [have_dst], 1
.pa_ok:
    clc
    ret

skip_ws:
.sw:
    mov al, [si]
    cmp al, ' '
    je .sw_skip
    cmp al, 9
    je .sw_skip
    cmp al, 13
    je .sw_end
    cmp al, 0
    je .sw_end
    clc
    ret
.sw_skip:
    inc si
    jmp .sw
.sw_end:
    stc
    ret

copy_tok:
    mov cx, 63
.ct:
    mov al, [si]
    cmp al, ' '
    je .ct_done
    cmp al, 9
    je .ct_done
    cmp al, 13
    je .ct_done
    cmp al, 0
    je .ct_done
    mov [di], al
    inc si
    inc di
    loop .ct
.ct_done:
    mov byte ptr [di], 0
    cmp cx, 63
    je .ct_empty
    clc
    ret
.ct_empty:
    stc
    ret

gunzip_member:
    mov ah, 0x3F
    mov bx, word ptr [in_handle]
    mov cx, 10
    lea dx, [hdr]
    int 0x21
    jc .gm_fail
    cmp ax, 10
    jne .gm_fail
    cmp byte ptr [hdr], 0x1F
    jne .gm_fail
    cmp byte ptr [hdr+1], 0x8B
    jne .gm_fail
    cmp byte ptr [hdr+2], 8
    jne .gm_fail
    mov al, byte ptr [hdr+3]
    test al, al
    jnz .gm_fail

    call inflate_stream
    jc .gm_fail

    call inf_byte_align
    call inf_pull_byte
    jc .gm_fail
    mov byte ptr [trail], al
    call inf_pull_byte
    jc .gm_fail
    mov byte ptr [trail+1], al
    call inf_pull_byte
    jc .gm_fail
    mov byte ptr [trail+2], al
    call inf_pull_byte
    jc .gm_fail
    mov byte ptr [trail+3], al
    call inf_pull_byte
    jc .gm_fail
    mov byte ptr [trail+4], al
    call inf_pull_byte
    jc .gm_fail
    mov byte ptr [trail+5], al
    call inf_pull_byte
    jc .gm_fail
    mov byte ptr [trail+6], al
    call inf_pull_byte
    jc .gm_fail
    mov byte ptr [trail+7], al

    mov ax, word ptr [trail]
    cmp ax, word ptr [crc32_lo]
    jne .gm_fail
    mov ax, word ptr [trail+2]
    cmp ax, word ptr [crc32_hi]
    jne .gm_fail
    mov ax, word ptr [trail+4]
    cmp ax, word ptr [isize_lo]
    jne .gm_fail
    mov ax, word ptr [trail+6]
    cmp ax, word ptr [isize_hi]
    jne .gm_fail
    clc
    ret
.gm_fail:
    stc
    ret

in_handle:   .word 0
out_handle:  .word 0
win_seg:     .word 0
have_src:    .byte 0
have_dst:    .byte 0
in_is_file:  .byte 0
out_is_file: .byte 0

src_path:   .space 64, 0
dst_path:   .space 64, 0
hdr:        .space 10, 0
trail:      .space 8, 0

msg_ok:  .ascii "gunzipped\r\n$"
msg_e:   .ascii "GUNZIP failed\r\n$"
msg_mem: .ascii "GUNZIP: out of memory\r\n$"

.include "firmware/src/dos/inc/crc32.inc"
.include "firmware/src/dos/inc/inflate.inc"
