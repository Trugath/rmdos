.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * ANSI.SYS — character device CON filter (CSI + SGR).
 * Loaded by DEVICE=; header at offset 0.
 */

.equ DEV_CMD_INIT, 0
.equ DEV_CMD_INPUT, 4
.equ DEV_CMD_INSTAT, 5
.equ DEV_CMD_OUTPUT, 8
.equ DEV_CMD_OUTSTAT, 10
.equ DEV_STAT_DONE, 0x0100

_start:
ansi_hdr:
    .word 0xFFFF
    .word 0xFFFF
    .word 0x8003                 /* char + stdin + stdout */
    .word offset ansi_strategy
    .word offset ansi_interrupt
    .ascii "CON     "

ansi_strategy:
    mov word ptr cs:[ansi_rh_off], bx
    mov word ptr cs:[ansi_rh_seg], es
    retf

ansi_interrupt:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push bp
    push ds
    push es
    push cs
    pop ds
    mov es, word ptr [ansi_rh_seg]
    mov bx, word ptr [ansi_rh_off]
    mov al, es:[bx + 2]
    cmp al, DEV_CMD_INIT
    je .ai_init
    cmp al, DEV_CMD_INPUT
    je .ai_in
    cmp al, DEV_CMD_INSTAT
    je .ai_istat
    cmp al, DEV_CMD_OUTPUT
    je .ai_out
    cmp al, DEV_CMD_OUTSTAT
    je .ai_ostat
    mov word ptr es:[bx + 3], DEV_STAT_DONE
    jmp .ai_done
.ai_init:
    /* save next driver for INPUT forward */
    mov ax, word ptr [ansi_hdr]
    mov word ptr [ansi_next_off], ax
    mov ax, word ptr [ansi_hdr + 2]
    mov word ptr [ansi_next_seg], ax
    lea ax, [ansi_image_end]
    mov es:[bx + 0x0E], ax
    mov es:[bx + 0x10], cs
    mov byte ptr [ansi_state], 0
    mov byte ptr [ansi_attr], 0x07
    mov word ptr es:[bx + 3], DEV_STAT_DONE
    jmp .ai_done
.ai_istat:
    /* forward to next if present, else INT 16 */
    cmp word ptr [ansi_next_seg], 0xFFFF
    je .ai_istat_loc
    call ansi_forward
    jmp .ai_done
.ai_istat_loc:
    mov ah, 0x01
    int 0x16
    mov ax, DEV_STAT_DONE
    jz .ai_istat_busy
    mov word ptr es:[bx + 3], ax
    jmp .ai_done
.ai_istat_busy:
    or ax, 0x0200
    mov word ptr es:[bx + 3], ax
    jmp .ai_done
.ai_in:
    /* Remap uses local INT 16; otherwise forward to next CON */
    cmp byte ptr cs:[ansi_map_en], 0
    jne .ai_in_loc
    cmp word ptr [ansi_next_seg], 0xFFFF
    je .ai_in_loc
    call ansi_forward
    jmp .ai_done
.ai_in_loc:
    push ds
    lds si, es:[bx + 0x0D]
    mov cx, es:[bx + 0x11]
    jcxz .ai_in_ok
.ai_in_lp:
    mov ah, 0x00
    int 0x16
    cmp byte ptr cs:[ansi_map_en], 0
    je .ai_in_store
    cmp ah, byte ptr cs:[ansi_map_sc]
    jne .ai_in_store
    mov al, byte ptr cs:[ansi_map_ch]
.ai_in_store:
    mov [si], al
    inc si
    loop .ai_in_lp
.ai_in_ok:
    pop ds
    mov word ptr es:[bx + 3], DEV_STAT_DONE
    jmp .ai_done
.ai_ostat:
    mov word ptr es:[bx + 3], DEV_STAT_DONE
    jmp .ai_done
.ai_out:
    push ds
    lds si, es:[bx + 0x0D]
    mov cx, es:[bx + 0x11]
    jcxz .ai_out_ok
.ai_out_lp:
    lodsb
    call ansi_putc
    loop .ai_out_lp
.ai_out_ok:
    pop ds
    mov word ptr es:[bx + 3], DEV_STAT_DONE
.ai_done:
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    retf

/* Forward current request (ansi_rh) to next driver. */
ansi_forward:
    push ax
    push bx
    push si
    push es
    push ds
    mov es, word ptr cs:[ansi_rh_seg]
    mov bx, word ptr cs:[ansi_rh_off]
    mov ax, word ptr cs:[ansi_next_seg]
    mov ds, ax
    mov si, word ptr cs:[ansi_next_off]
    mov ax, [si + 6]
    mov word ptr cs:[ansi_far_off], ax
    mov word ptr cs:[ansi_far_seg], ds
    push cs
    pop ds
    call ansi_far_invoke
    mov es, word ptr cs:[ansi_rh_seg]
    mov bx, word ptr cs:[ansi_rh_off]
    mov ax, word ptr cs:[ansi_next_seg]
    mov ds, ax
    mov si, word ptr cs:[ansi_next_off]
    mov ax, [si + 8]
    mov word ptr cs:[ansi_far_off], ax
    mov word ptr cs:[ansi_far_seg], ds
    push cs
    pop ds
    call ansi_far_invoke
    pop ds
    pop es
    pop si
    pop bx
    pop ax
    ret

ansi_far_invoke:
    push cs
    lea ax, [.afi_ret]
    push ax
    push word ptr cs:[ansi_far_seg]
    push word ptr cs:[ansi_far_off]
    retf
.afi_ret:
    ret

/*
 * AL = next output byte. State machine; printable → screen (+ COM1).
 * ESC/CSI bytes are consumed (not mirrored to COM1).
 */
ansi_putc:
    push ax
    push bx
    push cx
    push dx
    mov bl, byte ptr cs:[ansi_state]
    cmp bl, 0
    je .ap_norm
    cmp bl, 1
    je .ap_esc
    jmp .ap_csi

.ap_norm:
    cmp al, 0x1B
    jne .ap_emit
    mov byte ptr cs:[ansi_state], 1
    jmp .ap_done
.ap_emit:
    call ansi_emit
    jmp .ap_done

.ap_esc:
    cmp al, '['
    je .ap_esc_csi
    cmp al, '7'
    je .ap_esc_save
    cmp al, '8'
    je .ap_esc_rest
    /* unknown ESC-x: emit x as text */
    mov byte ptr cs:[ansi_state], 0
    call ansi_emit
    jmp .ap_done
.ap_esc_csi:
    mov byte ptr cs:[ansi_state], 2
    mov word ptr cs:[ansi_p1], 0
    mov word ptr cs:[ansi_p2], 0
    mov byte ptr cs:[ansi_pwhich], 0
    mov byte ptr cs:[ansi_phave], 0
    jmp .ap_done
.ap_esc_save:
    call ansi_get_cur
    mov byte ptr cs:[ansi_save_row], dh
    mov byte ptr cs:[ansi_save_col], dl
    mov byte ptr cs:[ansi_state], 0
    jmp .ap_done
.ap_esc_rest:
    mov dh, byte ptr cs:[ansi_save_row]
    mov dl, byte ptr cs:[ansi_save_col]
    call ansi_set_cur
    mov byte ptr cs:[ansi_state], 0
    jmp .ap_done

.ap_csi:
    cmp al, ';'
    je .ap_csi_semi
    cmp al, '0'
    jb .ap_csi_final
    cmp al, '9'
    ja .ap_csi_final
    /* digit */
    mov byte ptr cs:[ansi_phave], 1
    xor ah, ah
    sub al, '0'
    mov bx, ax
    cmp byte ptr cs:[ansi_pwhich], 0
    jne .ap_csi_d2
    mov ax, word ptr cs:[ansi_p1]
    mov cx, 10
    mul cx
    add ax, bx
    mov word ptr cs:[ansi_p1], ax
    jmp .ap_done
.ap_csi_d2:
    mov ax, word ptr cs:[ansi_p2]
    mov cx, 10
    mul cx
    add ax, bx
    mov word ptr cs:[ansi_p2], ax
    jmp .ap_done
.ap_csi_semi:
    mov byte ptr cs:[ansi_pwhich], 1
    mov byte ptr cs:[ansi_phave], 1
    jmp .ap_done
.ap_csi_final:
    mov byte ptr cs:[ansi_state], 0
    call ansi_csi_cmd
.ap_done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* AL = final CSI byte; params in ansi_p1/p2. */
ansi_csi_cmd:
    cmp al, 'H'
    je .acc_cup
    cmp al, 'f'
    je .acc_cup
    cmp al, 'A'
    je .acc_cuu
    cmp al, 'B'
    je .acc_cud
    cmp al, 'C'
    je .acc_cuf
    cmp al, 'D'
    je .acc_cub
    cmp al, 'G'
    je .acc_cha
    cmp al, 'J'
    je .acc_ed
    cmp al, 'K'
    je .acc_el
    cmp al, 'm'
    je .acc_sgr
    cmp al, 'h'
    je .acc_ret
    cmp al, 'l'
    je .acc_ret
    cmp al, 'p'
    je .acc_key
.acc_ret:
    ret

/* Tiny key remap: CSI ... p enables F1 (sc 0x3B) → '!' */
.acc_key:
    mov byte ptr cs:[ansi_map_en], 1
    mov byte ptr cs:[ansi_map_sc], 0x3B
    mov byte ptr cs:[ansi_map_ch], '!'
    ret

.acc_cup:
    call ansi_param1
    test ax, ax
    jnz .acc_cup_r
    mov ax, 1
.acc_cup_r:
    dec ax
    cmp ax, 24
    jbe .acc_cup_rok
    mov ax, 24
.acc_cup_rok:
    mov dh, al
    call ansi_param2
    test ax, ax
    jnz .acc_cup_c
    mov ax, 1
.acc_cup_c:
    dec ax
    cmp ax, 79
    jbe .acc_cup_cok
    mov ax, 79
.acc_cup_cok:
    mov dl, al
    call ansi_set_cur
    ret

.acc_cuu:
    call ansi_param1_def1
    call ansi_get_cur
    sub dh, al
    jnc .acc_cuu_ok
    xor dh, dh
.acc_cuu_ok:
    call ansi_set_cur
    ret
.acc_cud:
    call ansi_param1_def1
    call ansi_get_cur
    add dh, al
    cmp dh, 24
    jbe .acc_cud_ok
    mov dh, 24
.acc_cud_ok:
    call ansi_set_cur
    ret
.acc_cuf:
    call ansi_param1_def1
    call ansi_get_cur
    add dl, al
    cmp dl, 79
    jbe .acc_cuf_ok
    mov dl, 79
.acc_cuf_ok:
    call ansi_set_cur
    ret
.acc_cub:
    call ansi_param1_def1
    call ansi_get_cur
    sub dl, al
    jnc .acc_cub_ok
    xor dl, dl
.acc_cub_ok:
    call ansi_set_cur
    ret
.acc_cha:
    call ansi_param1
    test ax, ax
    jnz .acc_cha_n
    mov ax, 1
.acc_cha_n:
    dec ax
    cmp ax, 79
    jbe .acc_cha_ok
    mov ax, 79
.acc_cha_ok:
    call ansi_get_cur
    mov dl, al
    call ansi_set_cur
    ret

.acc_ed:
    call ansi_param1
    /* 0/omit = EOS, 1 = BOS, 2 = all */
    cmp ax, 2
    je .acc_ed_all
    cmp ax, 1
    je .acc_ed_bos
    /* EOS from cursor */
    call ansi_get_cur
    mov ah, 0x06
    mov al, 0
    mov bh, byte ptr cs:[ansi_attr]
    mov ch, dh
    mov cl, dl
    mov dh, 24
    mov dl, 79
    int 0x10
    ret
.acc_ed_bos:
    call ansi_get_cur
    mov ah, 0x06
    mov al, 0
    mov bh, byte ptr cs:[ansi_attr]
    xor cx, cx
    /* DX = cursor */
    int 0x10
    ret
.acc_ed_all:
    mov ah, 0x06
    mov al, 0
    mov bh, byte ptr cs:[ansi_attr]
    xor cx, cx
    mov dx, 0x184F
    int 0x10
    xor dx, dx
    call ansi_set_cur
    ret

.acc_el:
    call ansi_param1
    call ansi_get_cur
    push dx
    cmp ax, 2
    je .acc_el_all
    cmp ax, 1
    je .acc_el_bol
    /* EOL */
    mov ah, 0x06
    mov al, 0
    mov bh, byte ptr cs:[ansi_attr]
    mov ch, dh
    mov cl, dl
    mov dl, 79
    int 0x10
    pop dx
    call ansi_set_cur
    ret
.acc_el_bol:
    mov ah, 0x06
    mov al, 0
    mov bh, byte ptr cs:[ansi_attr]
    mov ch, dh
    xor cl, cl
    /* DX already cursor */
    int 0x10
    pop dx
    call ansi_set_cur
    ret
.acc_el_all:
    mov ah, 0x06
    mov al, 0
    mov bh, byte ptr cs:[ansi_attr]
    mov ch, dh
    xor cl, cl
    mov dl, 79
    int 0x10
    pop dx
    call ansi_set_cur
    ret

.acc_sgr:
    /* Apply p1 then p2 if present (simple two-param). */
    mov ax, word ptr cs:[ansi_p1]
    call ansi_apply_sgr
    cmp byte ptr cs:[ansi_pwhich], 0
    je .acc_ret
    mov ax, word ptr cs:[ansi_p2]
    call ansi_apply_sgr
    ret

/* AX = SGR param */
ansi_apply_sgr:
    test ax, ax
    jnz .aas_n0
    mov byte ptr cs:[ansi_attr], 0x07
    ret
.aas_n0:
    cmp ax, 1
    jne .aas_fg
    or byte ptr cs:[ansi_attr], 0x08
    ret
.aas_fg:
    cmp ax, 30
    jb .aas_bg
    cmp ax, 37
    ja .aas_bg
    sub ax, 30
    mov bx, ax
    mov al, byte ptr cs:[ansi_ansi2cga + bx]
    mov ah, byte ptr cs:[ansi_attr]
    and ah, 0xF8
    or ah, al
    mov byte ptr cs:[ansi_attr], ah
    ret
.aas_bg:
    cmp ax, 40
    jb .aas_done
    cmp ax, 47
    ja .aas_done
    sub ax, 40
    mov bx, ax
    mov al, byte ptr cs:[ansi_ansi2cga + bx]
    mov cl, 4
    shl al, cl
    mov ah, byte ptr cs:[ansi_attr]
    and ah, 0x8F
    or ah, al
    mov byte ptr cs:[ansi_attr], ah
.aas_done:
    ret

ansi_ansi2cga:
    .byte 0, 4, 2, 6, 1, 5, 3, 7

ansi_param1:
    mov ax, word ptr cs:[ansi_p1]
    cmp byte ptr cs:[ansi_phave], 0
    jne .ap1_ok
    xor ax, ax
.ap1_ok:
    ret

ansi_param2:
    mov ax, word ptr cs:[ansi_p2]
    ret

ansi_param1_def1:
    call ansi_param1
    test ax, ax
    jnz .apd_ok
    mov ax, 1
.apd_ok:
    /* return count in AL (clamp 255) */
    cmp ax, 255
    jbe .apd_ret
    mov ax, 255
.apd_ret:
    ret

ansi_get_cur:
    push ax
    push bx
    mov ah, 0x03
    xor bh, bh
    int 0x10
    pop bx
    pop ax
    ret

ansi_set_cur:
    push ax
    push bx
    mov ah, 0x02
    xor bh, bh
    int 0x10
    pop bx
    pop ax
    ret

/* Emit AL to screen (+ COM1). Uses ansi_attr. */
ansi_emit:
    push ax
    push bx
    push cx
    push dx
    mov byte ptr cs:[ansi_emit_ch], al
    cmp al, 7
    je .ae_bel
    cmp al, 8
    je .ae_bs
    cmp al, 10
    je .ae_lf
    cmp al, 13
    je .ae_cr
    cmp byte ptr cs:[ansi_attr], 0x07
    jne .ae_attr
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    jmp .ae_com1
.ae_attr:
    call ansi_get_cur
    mov al, byte ptr cs:[ansi_emit_ch]
    mov ah, 0x09
    mov bl, byte ptr cs:[ansi_attr]
    xor bh, bh
    mov cx, 1
    int 0x10
    call ansi_get_cur
    inc dl
    cmp dl, 80
    jb .ae_adv
    xor dl, dl
    inc dh
    cmp dh, 25
    jb .ae_adv
    mov ah, 0x06
    mov al, 1
    mov bh, byte ptr cs:[ansi_attr]
    xor cx, cx
    mov dx, 0x184F
    int 0x10
    mov dh, 24
    xor dl, dl
.ae_adv:
    call ansi_set_cur
    jmp .ae_com1
.ae_bel:
.ae_bs:
.ae_lf:
.ae_cr:
    mov al, byte ptr cs:[ansi_emit_ch]
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
.ae_com1:
    mov ah, byte ptr cs:[ansi_emit_ch]
    push dx
.ae_tx:
    mov dx, 0x3FD
    in al, dx
    test al, 0x20
    jz .ae_tx
    mov al, ah
    mov dx, 0x3F8
    out dx, al
    pop dx
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* Resident data */
ansi_rh_off:
    .word 0
ansi_rh_seg:
    .word 0
ansi_next_off:
    .word 0xFFFF
ansi_next_seg:
    .word 0xFFFF
ansi_far_off:
    .word 0
ansi_far_seg:
    .word 0
ansi_state:
    .byte 0
ansi_attr:
    .byte 0x07
ansi_p1:
    .word 0
ansi_p2:
    .word 0
ansi_pwhich:
    .byte 0
ansi_phave:
    .byte 0
ansi_save_row:
    .byte 0
ansi_save_col:
    .byte 0
ansi_emit_ch:
    .byte 0
ansi_map_en:
    .byte 0
ansi_map_sc:
    .byte 0
ansi_map_ch:
    .byte 0

ansi_image_end:
