.code16
.intel_syntax noprefix

.include "firmware/bios/inc/equates.inc"

/*
 * rmDOS clean-room XT system BIOS — keyboard:
 * IRQ1 handler, scancode translation, BDA ring buffer, INT 16h services.
 */

.section .text
.global isr_09, int16_handler

isr_09:
    push ax
    push bx
    push cx
    push ds
    mov ax, BDA_SEG
    mov ds, ax

    in al, PORT_PPI_A
    mov ah, al

    in al, PORT_PPI_B
    mov bl, al
    or al, 0x80
    out PORT_PPI_B, al
    mov al, bl
    out PORT_PPI_B, al

    test ah, 0x80
    jnz .k09_break

    cmp ah, 0x53
    jne .k09_flags
    test byte ptr [BDA_KBD_FLAG0], 0x0C
    jz .k09_flags
    mov word ptr [BDA_WARM_FLAG], WARM_BOOT_MAGIC
    jmp 0xF000:0xEA82

.k09_flags:
    cmp ah, 0x1D                      /* Ctrl */
    jne .k09_not_ctrl
    or byte ptr [BDA_KBD_FLAG0], 0x04
    jmp .k09_eoi
.k09_not_ctrl:
    cmp ah, 0x38                      /* Alt */
    jne .k09_not_alt
    or byte ptr [BDA_KBD_FLAG0], 0x08
    jmp .k09_eoi
.k09_not_alt:
    cmp ah, 0x2A                      /* Left Shift */
    jne .k09_not_lshift
    or byte ptr [BDA_KBD_FLAG0], 0x02
    jmp .k09_eoi
.k09_not_lshift:
    cmp ah, 0x36                      /* Right Shift */
    jne .k09_not_shift
    or byte ptr [BDA_KBD_FLAG0], 0x01
    jmp .k09_eoi
.k09_not_shift:
    cmp ah, 0x3A                      /* Caps Lock */
    jne .k09_not_caps
    xor byte ptr [BDA_KBD_FLAG0], 0x40
    jmp .k09_eoi
.k09_not_caps:
    cmp ah, 0x45                      /* Num Lock */
    jne .k09_not_num
    xor byte ptr [BDA_KBD_FLAG0], 0x20
    jmp .k09_eoi
.k09_not_num:
    cmp ah, 0x46                      /* Scroll Lock */
    jne .k09_post_esc
    xor byte ptr [BDA_KBD_FLAG0], 0x10
    jmp .k09_eoi

.k09_post_esc:
    /* During POST, Esc make latches skip — RAM test sees it between chunks. */
    cmp ah, 0x01                      /* Esc */
    jne .k09_enqueue
    push es
    xor bx, bx
    mov es, bx
    cmp byte ptr es:[POST_ACTIVE], 0
    je .k09_esc_queue
    mov byte ptr es:[POST_SKIP], 1
    pop es
    jmp .k09_eoi                      /* swallow Esc during POST */
.k09_esc_queue:
    pop es
    jmp .k09_enqueue

.k09_break:
    and ah, 0x7F
    cmp ah, 0x1D
    jne .k09_brk_alt
    and byte ptr [BDA_KBD_FLAG0], 0xFB
    jmp .k09_eoi
.k09_brk_alt:
    cmp ah, 0x38
    jne .k09_brk_shift
    and byte ptr [BDA_KBD_FLAG0], 0xF7
    jmp .k09_eoi
.k09_brk_shift:
    cmp ah, 0x2A
    jne .k09_brk_rshift
    and byte ptr [BDA_KBD_FLAG0], 0xFD
    jmp .k09_eoi
.k09_brk_rshift:
    cmp ah, 0x36
    jne .k09_eoi
    and byte ptr [BDA_KBD_FLAG0], 0xFE
    jmp .k09_eoi

.k09_enqueue:
    /* Keypad Ins (52h) with NumLock off: toggle Insert flag. */
    cmp ah, 0x52
    jne .k09_do_xlat
    test byte ptr [BDA_KBD_FLAG0], 0x20
    jnz .k09_do_xlat
    xor byte ptr [BDA_KBD_FLAG0], 0x80
.k09_do_xlat:
    call scancode_to_ascii
    call kbd_enqueue

.k09_eoi:
    mov al, 0x20
    out PORT_PIC_CMD, al
    pop ds
    pop cx
    pop bx
    pop ax
    iret

scancode_to_ascii:
    push bx
    push cx
    push dx
    push ds
    push es
    push cs
    pop ds
    mov al, ah
    /* Keypad 47h–53h: digits when NumLock, else AL=0 (extended). */
    cmp al, 0x47
    jb .sc_main
    cmp al, 0x53
    ja .sc_zero
    mov dx, BDA_SEG
    mov es, dx
    test byte ptr es:[BDA_KBD_FLAG0], 0x20
    jz .sc_zero
    mov bx, offset keypad_num_table
    sub al, 0x47
    xlat
    jmp .sc_done
.sc_main:
    cmp al, 0x3A
    ja .sc_zero
    mov bx, offset scancode_table
    xlat
    mov cl, al
    mov dx, BDA_SEG
    mov es, dx
    mov bl, es:[BDA_KBD_FLAG0]
    mov al, cl
    /* Ctrl+letter → C0 control (Ctrl+C = 0x03) */
    test bl, 0x04
    jz .sc_shift
    cmp al, 'a'
    jb .sc_ctrl_up
    cmp al, 'z'
    ja .sc_ctrl_up
    sub al, 0x60
    jmp .sc_done
.sc_ctrl_up:
    cmp al, 'A'
    jb .sc_ctrl_other
    cmp al, 'Z'
    ja .sc_ctrl_other
    and al, 0x1F
    jmp .sc_done
.sc_ctrl_other:
    /* keep punctuation; Shift still applies below if needed */
.sc_shift:
    test bl, 0x03
    jz .sc_caps
    cmp al, 'a'
    jb .sc_shift_sym
    cmp al, 'z'
    ja .sc_shift_sym
    sub al, 0x20
    jmp .sc_done
.sc_shift_sym:
    cmp al, '1'
    jb .sc_done
    cmp al, '9'
    ja .sc_check0
    mov bx, offset shift_digit_table
    sub al, '1'
    xlat
    jmp .sc_done
.sc_check0:
    cmp al, '0'
    jne .sc_done
    mov al, ')'
    jmp .sc_done
.sc_caps:
    test bl, 0x40
    jz .sc_done
    cmp al, 'a'
    jb .sc_done
    cmp al, 'z'
    ja .sc_done
    sub al, 0x20
    jmp .sc_done
.sc_zero:
    xor al, al
.sc_done:
    pop es
    pop ds
    pop dx
    pop cx
    pop bx
    ret

kbd_enqueue:
    push si
    push di
    mov si, [BDA_KBD_BUF_TAIL]
    mov di, si
    add di, 2
    cmp di, [BDA_KBD_BUF_ENDPTR]
    jb .kq_wrap_ok
    mov di, [BDA_KBD_BUF_START]
.kq_wrap_ok:
    cmp di, [BDA_KBD_BUF_HEAD]
    je .kq_full
    mov [si], ax
    mov [BDA_KBD_BUF_TAIL], di
.kq_full:
    pop di
    pop si
    ret

int16_handler:
    sti
    cmp ah, 0x00
    je .i16_read
    cmp ah, 0x01
    je .i16_status
    cmp ah, 0x02
    je .i16_shift
    xor ah, ah
    iret

.i16_read:
    push bx
    push ds
    mov ax, BDA_SEG
    mov ds, ax
.i16_wait:
    mov bx, [BDA_KBD_BUF_HEAD]
    cmp bx, [BDA_KBD_BUF_TAIL]
    jne .i16_got
    hlt
    jmp .i16_wait
.i16_got:
    mov ax, [bx]
    add bx, 2
    cmp bx, [BDA_KBD_BUF_ENDPTR]
    jb .i16_nowrap
    mov bx, [BDA_KBD_BUF_START]
.i16_nowrap:
    mov [BDA_KBD_BUF_HEAD], bx
    pop ds
    pop bx
    iret

.i16_status:
    push bx
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov bx, [BDA_KBD_BUF_HEAD]
    cmp bx, [BDA_KBD_BUF_TAIL]
    mov ax, [bx]
    pop ds
    push bp
    mov bp, sp
    /* flags at [bp+8] after push bp; BX already pushed below return frame —
     * stack: [bp]=saved bp, [bp+2]=ip, [bp+4]=cs, [bp+6]=flags from INT.
     * We also pushed BX before DS; after pop ds, SP points at saved BX.
     * After push bp: [bp+0]=old bp, [bp+2]=saved bx, [bp+4]=ip, [bp+6]=cs, [bp+8]=flags
     */
    je .i16_empty
    and word ptr [bp + 8], 0xFFBF
    jmp .i16_st_done
.i16_empty:
    or word ptr [bp + 8], 0x0040
    xor ax, ax
.i16_st_done:
    pop bp
    pop bx
    iret

.i16_shift:
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov al, [BDA_KBD_FLAG0]
    pop ds
    iret

scancode_table:
    .byte 0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', 8, 9
    .byte 'q','w','e','r','t','y','u','i','o','p','[',']', 13, 0
    .byte 'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\'
    .byte 'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' '

/* shifted '1'..'9' */
shift_digit_table:
    .byte '!','@','#','$','%','^','&','*','('

/* keypad make 47h–53h ASCII when NumLock on */
keypad_num_table:
    .byte '7','8','9','-','4','5','6','+','1','2','3','0','.'
