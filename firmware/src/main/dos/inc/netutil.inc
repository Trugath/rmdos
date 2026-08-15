/*
 * Shared net console / Ctrl-C helpers for PING and DHCP.
 * Caller data: abort_flag. break_isr writes cs:[abort_flag].
 */

install_break:
    push ax
    push dx
    mov ax, 0x2523
    lea dx, [break_isr]
    int 0x21
    /* enable Ctrl-C checks */
    mov ax, 0x3301
    mov dl, 1
    int 0x21
    pop dx
    pop ax
    ret

break_isr:
    mov byte ptr cs:[abort_flag], 1
    iret

check_abort:
    cmp byte ptr cs:[abort_flag], 0
    jne .nu_ca_yes
    push ax
    push bx
    push ds
    push cs
    pop ds
    mov ah, 0x01
    int 0x16
    jz .nu_ca_dos
    cmp al, 0x03
    je .nu_ca_take
    mov bl, al
    mov ax, 0x40
    mov ds, ax
    test byte ptr [0x17], 0x04
    jz .nu_ca_dos_rest
    push cs
    pop ds
    or bl, 0x20
    cmp bl, 'c'
    jne .nu_ca_dos
.nu_ca_take:
    push cs
    pop ds
    mov ah, 0x00
    int 0x16
    mov byte ptr [abort_flag], 1
    jmp .nu_ca_no
.nu_ca_dos_rest:
    push cs
    pop ds
.nu_ca_dos:
    mov ah, 0x0B
    int 0x21
.nu_ca_no:
    pop ds
    pop bx
    pop ax
    cmp byte ptr cs:[abort_flag], 0
    ret
.nu_ca_yes:
    ret

get_ticks:
    push ds
    mov ax, 0x40
    mov ds, ax
    mov ax, [0x6C]
    pop ds
    ret

/* Print 4 bytes at SI as dotted decimal (preserves SI). */
print_ip:
    push ax
    push bx
    push cx
    push dx
    push si
    mov cx, 4
.nu_pi:
    xor ax, ax
    lodsb
    call print_u16
    dec cx
    jz .nu_pi_done
    mov ah, 0x02
    mov dl, '.'
    int 0x21
    jmp .nu_pi
.nu_pi_done:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* Print AX as unsigned decimal. */
print_u16:
    push ax
    push bx
    push cx
    push dx
    mov bx, 10
    xor cx, cx
    test ax, ax
    jnz .nu_pu_div
    mov ah, 0x02
    mov dl, '0'
    int 0x21
    jmp .nu_pu_done
.nu_pu_div:
    xor dx, dx
    div bx
    push dx
    inc cx
    test ax, ax
    jnz .nu_pu_div
.nu_pu_out:
    pop dx
    add dl, '0'
    mov ah, 0x02
    int 0x21
    loop .nu_pu_out
.nu_pu_done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret
