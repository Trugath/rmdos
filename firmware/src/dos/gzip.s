.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * GZIP.COM — compress to a gzip member.
 * Usage: GZIP [src [dst]]
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
    call gzip_member
    jc fail_data

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

fail_data:
    call close_handles
    mov ah, 0x09
    lea dx, [msg_e]
    int 0x21
    mov ax, 0x4C01
    int 0x21

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

/* Parse 0–2 path args into src/dst; set have_src / have_dst. */
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

gzip_member:
    mov ah, 0x40
    mov bx, word ptr [out_handle]
    mov cx, 10
    lea dx, [gz_hdr]
    int 0x21
    jc .gz_fail
    cmp ax, 10
    jne .gz_fail

    call deflate_stream
    jc .gz_fail

    mov ax, word ptr [crc32_lo]
    mov word ptr [gz_trail], ax
    mov ax, word ptr [crc32_hi]
    mov word ptr [gz_trail+2], ax
    mov ax, word ptr [isize_lo]
    mov word ptr [gz_trail+4], ax
    mov ax, word ptr [isize_hi]
    mov word ptr [gz_trail+6], ax

    mov ah, 0x40
    mov bx, word ptr [out_handle]
    mov cx, 8
    lea dx, [gz_trail]
    int 0x21
    jc .gz_fail
    cmp ax, 8
    jne .gz_fail
    clc
    ret
.gz_fail:
    stc
    ret

in_handle:   .word 0
out_handle:  .word 0
have_src:    .byte 0
have_dst:    .byte 0
in_is_file:  .byte 0
out_is_file: .byte 0

src_path:   .space 64, 0
dst_path:   .space 64, 0
gz_trail:   .space 8, 0

gz_hdr:
    .byte 0x1F, 0x8B, 0x08, 0x00
    .byte 0x00, 0x00, 0x00, 0x00
    .byte 0x00, 0xFF

msg_ok:  .ascii "gzipped\r\n$"
msg_e:   .ascii "GZIP failed\r\n$"

.include "firmware/src/dos/inc/crc32.inc"
.include "firmware/src/dos/inc/deflate.inc"
