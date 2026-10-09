.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * MOUSE.COM — clean-room Microsoft serial mouse driver (COM1) + INT 33h TSR.
 * 1200 7N1 on BDA COM1 (40:00). Polls RBR from INT 33h (no IRQ4 / no INT 1Ch).
 * MOUSE /U — unload.
 */

_start:
    jmp install

/* ===================== resident ===================== */

/* Drain COM RBR while DR set. DS=CS. */
mouse_poll:
    push ax
    push dx
    mov dx, word ptr [com_base]
    add dx, 5
.mp_drain:
    in al, dx
    test al, 0x01
    jz .mp_done
    mov dx, word ptr [com_base]
    in al, dx
    call mouse_byte
    mov dx, word ptr [com_base]
    add dx, 5
    jmp .mp_drain
.mp_done:
    pop dx
    pop ax
    ret

mouse_byte:
    test al, 0x40
    jz .mb_data
    mov byte ptr [pkt_i], 1
    mov byte ptr [pkt0], al
    ret
.mb_data:
    cmp byte ptr [pkt_i], 1
    je .mb_b1
    cmp byte ptr [pkt_i], 2
    je .mb_b2
    ret
.mb_b1:
    mov byte ptr [pkt1], al
    mov byte ptr [pkt_i], 2
    ret
.mb_b2:
    mov byte ptr [pkt2], al
    mov byte ptr [pkt_i], 0
    jmp mouse_apply_pkt

mouse_apply_pkt:
    mov al, byte ptr [pkt0]
    xor bl, bl
    test al, 0x20
    jz .ap_nol
    or bl, 0x01
.ap_nol:
    test al, 0x10
    jz .ap_nor
    or bl, 0x02
.ap_nor:

    mov al, byte ptr [pkt0]
    and al, 0x03
    mov cl, 6
    shl al, cl
    mov ah, byte ptr [pkt1]
    and ah, 0x3F
    or al, ah
    cbw
    mov si, ax

    mov al, byte ptr [pkt0]
    and al, 0x0C
    mov cl, 4
    shl al, cl
    mov ah, byte ptr [pkt2]
    and ah, 0x3F
    or al, ah
    cbw
    mov di, ax

    add word ptr [mic_x], si
    add word ptr [mic_y], di

    mov ax, word ptr [pos_x]
    add ax, si
    cmp ax, word ptr [min_x]
    jge .ap_xmin
    mov ax, word ptr [min_x]
.ap_xmin:
    cmp ax, word ptr [max_x]
    jle .ap_xmax
    mov ax, word ptr [max_x]
.ap_xmax:
    mov word ptr [pos_x], ax

    /* Microsoft serial Y is up-positive; INT 33h screen Y is down-positive. */
    mov ax, word ptr [pos_y]
    sub ax, di
    cmp ax, word ptr [min_y]
    jge .ap_ymin
    mov ax, word ptr [min_y]
.ap_ymin:
    cmp ax, word ptr [max_y]
    jle .ap_ymax
    mov ax, word ptr [max_y]
.ap_ymax:
    mov word ptr [pos_y], ax

    xor cx, cx
    mov ax, si
    or ax, di
    jz .ap_nomove
    or cx, 0x01
.ap_nomove:
    mov al, bl
    mov ah, byte ptr [buttons]
    mov bh, al
    xor bh, ah
    test bh, 0x01
    jz .ap_nl
    test al, 0x01
    jz .ap_lr
    or cx, 0x02
    jmp .ap_nl
.ap_lr:
    or cx, 0x04
.ap_nl:
    test bh, 0x02
    jz .ap_nr
    test al, 0x02
    jz .ap_rr
    or cx, 0x08
    jmp .ap_nr
.ap_rr:
    or cx, 0x10
.ap_nr:
    mov byte ptr [buttons], al

    cmp byte ptr [cursor_on], 0
    je .ap_nocur
    call cursor_undraw
    call cursor_draw
.ap_nocur:

    mov ax, cx
    test ax, word ptr [call_mask]
    jz .ap_ret
    cmp word ptr [call_seg], 0
    je .ap_ret
    mov bx, word ptr [buttons]
    xor bh, bh
    mov cx, word ptr [pos_x]
    mov dx, word ptr [pos_y]
    pushf
    call dword ptr [call_off]
.ap_ret:
    ret

cursor_undraw:
    cmp byte ptr [cur_vis], 0
    je .cu_ret
    push ax
    push bx
    push cx
    push dx
    push ds
    mov ax, 0x40
    mov ds, ax
    cmp byte ptr [0x49], 4      /* BDA video mode */
    pop ds
    jb .cu_text
    /* Graphics modes: desk owns soft cursor; do not AH=09 glyph stamps. */
    mov byte ptr [cur_vis], 0
    pop dx
    pop cx
    pop bx
    pop ax
    ret
.cu_text:
    mov dh, byte ptr [cur_row]
    mov dl, byte ptr [cur_col]
    mov ah, 0x02
    xor bh, bh
    int 0x10
    mov ah, 0x09
    mov al, byte ptr [cur_char]
    mov bl, byte ptr [cur_attr]
    mov cx, 1
    xor bh, bh
    int 0x10
    mov byte ptr [cur_vis], 0
    pop dx
    pop cx
    pop bx
    pop ax
.cu_ret:
    ret

cursor_draw:
    push ax
    push bx
    push cx
    push dx
    push ds
    mov ax, 0x40
    mov ds, ax
    cmp byte ptr [0x49], 4
    pop ds
    jb .cd_text
    pop dx
    pop cx
    pop bx
    pop ax
    ret
.cd_text:
    mov ax, word ptr [pos_x]
    mov cl, 3
    shr ax, cl
    cmp ax, 79
    jbe .cd_xc
    mov ax, 79
.cd_xc:
    mov dl, al
    mov ax, word ptr [pos_y]
    mov cl, 3
    shr ax, cl
    cmp ax, 24
    jbe .cd_yc
    mov ax, 24
.cd_yc:
    mov dh, al
    mov byte ptr [cur_col], dl
    mov byte ptr [cur_row], dh
    mov ah, 0x02
    xor bh, bh
    int 0x10
    mov ah, 0x08
    int 0x10
    mov byte ptr [cur_char], al
    mov byte ptr [cur_attr], ah
    xor ah, 0x7F
    mov bl, ah
    mov ah, 0x09
    mov al, byte ptr [cur_char]
    mov cx, 1
    xor bh, bh
    int 0x10
    mov byte ptr [cur_vis], 1
    pop dx
    pop cx
    pop bx
    pop ax
    ret

mouse_int33:
    /* INT 33h function number is in AL (AX=00xx). */
    cmp al, 0x00
    je .i33_00
    cmp al, 0x01
    je .i33_01
    cmp al, 0x02
    je .i33_02
    cmp al, 0x03
    je .i33_03
    cmp al, 0x04
    je .i33_04
    cmp al, 0x07
    je .i33_07
    cmp al, 0x08
    je .i33_08
    cmp al, 0x0B
    je .i33_0B
    cmp al, 0x0C
    je .i33_0C
    iret

.i33_00:
    push ds
    push cs
    pop ds
    mov word ptr [pos_x], 320
    mov word ptr [pos_y], 100
    mov word ptr [min_x], 0
    mov word ptr [max_x], 639
    mov word ptr [min_y], 0
    mov word ptr [max_y], 199
    mov word ptr [mic_x], 0
    mov word ptr [mic_y], 0
    mov byte ptr [buttons], 0
    mov byte ptr [cursor_on], 0
    call cursor_undraw
    mov word ptr [call_mask], 0
    mov word ptr [call_off], 0
    mov word ptr [call_seg], 0
    mov byte ptr [pkt_i], 0
    pop ds
    mov ax, 0xFFFF
    mov bx, 2
    iret

.i33_01:
    push ds
    push cs
    pop ds
    mov byte ptr [cursor_on], 1
    call cursor_draw
    pop ds
    iret

.i33_02:
    push ds
    push cs
    pop ds
    mov byte ptr [cursor_on], 0
    call cursor_undraw
    pop ds
    iret

.i33_03:
    push ds
    push cs
    pop ds
    call mouse_poll
    xor bx, bx
    mov bl, byte ptr [buttons]
    mov cx, word ptr [pos_x]
    mov dx, word ptr [pos_y]
    pop ds
    iret

.i33_04:
    push ds
    push cs
    pop ds
    cmp cx, word ptr [min_x]
    jge .i04a
    mov cx, word ptr [min_x]
.i04a:
    cmp cx, word ptr [max_x]
    jle .i04b
    mov cx, word ptr [max_x]
.i04b:
    cmp dx, word ptr [min_y]
    jge .i04c
    mov dx, word ptr [min_y]
.i04c:
    cmp dx, word ptr [max_y]
    jle .i04d
    mov dx, word ptr [max_y]
.i04d:
    cmp byte ptr [cursor_on], 0
    je .i04e
    call cursor_undraw
.i04e:
    mov word ptr [pos_x], cx
    mov word ptr [pos_y], dx
    cmp byte ptr [cursor_on], 0
    je .i04f
    call cursor_draw
.i04f:
    pop ds
    iret

.i33_07:
    push ds
    push cs
    pop ds
    mov word ptr [min_x], cx
    mov word ptr [max_x], dx
    pop ds
    iret

.i33_08:
    push ds
    push cs
    pop ds
    mov word ptr [min_y], cx
    mov word ptr [max_y], dx
    pop ds
    iret

.i33_0B:
    push ds
    push cs
    pop ds
    call mouse_poll
    mov cx, word ptr [mic_x]
    mov dx, word ptr [mic_y]
    mov word ptr [mic_x], 0
    mov word ptr [mic_y], 0
    pop ds
    iret

.i33_0C:
    push ds
    push cs
    pop ds
    mov word ptr [call_mask], cx
    mov word ptr [call_off], dx
    mov word ptr [call_seg], es
    pop ds
    iret

com_base:   .word 0x3F8
pkt_i:      .byte 0
pkt0:       .byte 0
pkt1:       .byte 0
pkt2:       .byte 0
buttons:    .byte 0
cursor_on:  .byte 0
cur_vis:    .byte 0
cur_row:    .byte 0
cur_col:    .byte 0
cur_char:   .byte 0
cur_attr:   .byte 0x07
pos_x:      .word 320
pos_y:      .word 100
min_x:      .word 0
max_x:      .word 639
min_y:      .word 0
max_y:      .word 199
mic_x:      .word 0
mic_y:      .word 0
call_mask:  .word 0
call_off:   .word 0
call_seg:   .word 0
old_33:     .word 0, 0

tsr_end:

/* ===================== install ===================== */

install:
    mov si, 0x80
    mov cl, byte ptr [si]
    xor ch, ch
    jcxz .inst_check
    inc si
.inst_skip:
    lodsb
    cmp al, ' '
    je .inst_skip
    cmp al, 0x0D
    je .inst_check
    cmp al, '/'
    je .inst_slash
    loop .inst_skip
    jmp .inst_check
.inst_slash:
    lodsb
    and al, 0xDF
    cmp al, 'U'
    je do_unload

.inst_check:
    push es
    xor ax, ax
    mov es, ax
    cmp word ptr es:[0x33 * 4 + 2], 0
    pop es
    je .do_install
    xor ax, ax
    int 0x33
    cmp ax, 0xFFFF
    jne .do_install
    mov ah, 0x09
    lea dx, [msg_already]
    int 0x21
    mov ax, 0x4C01
    int 0x21

.do_install:
    push es
    mov ax, 0x40
    mov es, ax
    mov ax, word ptr es:[0]
    pop es
    test ax, ax
    jz .fail_nocom
    mov word ptr [com_base], ax

    mov ax, 0x3533
    int 0x21
    mov word ptr [old_33], bx
    mov word ptr [old_33 + 2], es

    mov dx, word ptr [com_base]
    add dx, 3
    mov al, 0x80
    out dx, al
    mov dx, word ptr [com_base]
    mov al, 0x60
    out dx, al
    inc dx
    xor al, al
    out dx, al
    mov dx, word ptr [com_base]
    add dx, 3
    mov al, 0x02
    out dx, al
    mov dx, word ptr [com_base]
    add dx, 4
    mov al, 0x0B
    out dx, al
    mov dx, word ptr [com_base]
    inc dx
    xor al, al
    out dx, al

    mov ax, 0x2533
    lea dx, [mouse_int33]
    int 0x21

    xor ax, ax
    int 0x33

    mov ah, 0x09
    lea dx, [msg_ok]
    int 0x21

    mov ax, word ptr [0x2C]
    test ax, ax
    jz .inst_keep
    mov es, ax
    mov ah, 0x49
    int 0x21
    mov word ptr [0x2C], 0

.inst_keep:
    lea dx, [tsr_end]
    add dx, 15
    mov cl, 4
    shr dx, cl
    mov ax, 0x3100
    int 0x21

.fail_nocom:
    mov ah, 0x09
    lea dx, [msg_nocom]
    int 0x21
    mov ax, 0x4C01
    int 0x21

do_unload:
    push es
    xor ax, ax
    mov es, ax
    cmp word ptr es:[0x33 * 4 + 2], 0
    pop es
    je .ul_none
    xor ax, ax
    int 0x33
    cmp ax, 0xFFFF
    jne .ul_none
    mov ax, 0x3533
    int 0x21
    mov ax, es
    mov bx, cs
    cmp ax, bx
    jne .ul_none

    mov ax, 0x0002
    int 0x33

    cli
    push ds
    push cs
    pop ds
    lds dx, dword ptr [old_33]
    mov ax, 0x2533
    int 0x21
    pop ds
    sti

    mov ax, cs
    mov es, ax
    mov ah, 0x49
    int 0x21

    mov ah, 0x09
    lea dx, [msg_unloaded]
    int 0x21
    mov ax, 0x4C00
    int 0x21

.ul_none:
    mov ah, 0x09
    lea dx, [msg_nounload]
    int 0x21
    mov ax, 0x4C01
    int 0x21

msg_ok:
    .ascii "MOUSE: serial mouse driver installed (COM1)\r\n$"
msg_already:
    .ascii "MOUSE: already installed\r\n$"
msg_nocom:
    .ascii "MOUSE: no COM1\r\n$"
msg_unloaded:
    .ascii "MOUSE: unloaded\r\n$"
msg_nounload:
    .ascii "MOUSE: not installed\r\n$"
