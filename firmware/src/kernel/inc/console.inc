/*
 * Console helpers + INT 21h character I/O (AH=01/02/09/0A/0B).
 * Assumes CS=DS for kernel data; INT 21h uses caller DS for string buffers.
 */

/* Teletype AL to CGA (INT 10h) and COM1. Preserves AL. */
putch:
    push ax
    push bx
    push dx
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    pop dx
    pop bx
    pop ax
    /* fall through */

com1_out:
    push ax
    push dx
    mov ah, al
.wait_tx:
    mov dx, 0x3FD
    in al, dx
    test al, 0x20
    jz .wait_tx
    mov al, ah
    mov dx, 0x3F8
    out dx, al
    pop dx
    pop ax
    ret

/* Print ASCIZ at DS:SI via putch. */
puts_asciz:
    push ax
    push si
.puts_a_loop:
    lodsb
    test al, al
    jz .puts_a_done
    call putch
    jmp .puts_a_loop
.puts_a_done:
    pop si
    pop ax
    ret

/* INT 21h AH=02 — write DL */
dos_ah02:
    mov al, dl
    call putch
    clc
    jmp int21_iret

/* INT 21h AH=09 — write DS:DX '$'-terminated */
dos_ah09:
    push ax
    push si
    mov si, dx
.ah09_loop:
    mov al, [si]
    cmp al, '$'
    je .ah09_done
    call putch
    inc si
    jmp .ah09_loop
.ah09_done:
    pop si
    pop ax
    clc
    jmp int21_iret

/* INT 21h AH=01 — read key with echo */
dos_ah01:
    call check_break
    mov ah, 0x00
    int 0x16
    cmp al, 0x0D
    jne .ah01_echo
    call putch
    mov al, 0x0A
    call putch
    mov al, 0x0D
    clc
    jmp int21_iret
.ah01_echo:
    call putch
    clc
    jmp int21_iret

/* INT 21h AH=06 — direct console I/O. DL=FF read nonblocking; else write DL. */
dos_ah06:
    cmp dl, 0xFF
    je .ah06_in
    mov al, dl
    call putch
    clc
    jmp int21_iret
.ah06_in:
    mov ah, 0x01
    int 0x16
    mov al, 0x00
    jz .ah06_none
    mov ah, 0x00
    int 0x16
    /* clear ZF in returned flags → char available */
    push bp
    mov bp, sp
    and byte ptr [bp + 6], 0xBF
    pop bp
    clc
    jmp int21_iret
.ah06_none:
    push bp
    mov bp, sp
    or byte ptr [bp + 6], 0x40
    pop bp
    clc
    jmp int21_iret

/* INT 21h AH=07 — raw input, no echo, no Ctrl-C check */
dos_ah07:
    mov ah, 0x00
    int 0x16
    clc
    jmp int21_iret

/* INT 21h AH=08 — input no echo, with Ctrl-C check */
dos_ah08:
    call check_break
    mov ah, 0x00
    int 0x16
    clc
    jmp int21_iret

/* INT 21h AH=0Bh — keyboard status */
dos_ah0b:
    call check_break
    mov ah, 0x01
    int 0x16
    mov al, 0x00
    jz .ah0b_none
    mov al, 0xFF
.ah0b_none:
    clc
    jmp int21_iret

/* INT 21h AH=0Ah — buffered input DS:DX */
dos_ah0a:
    push bx
    push cx
    push si
    push di
    mov di, dx
    mov cl, [di]
    mov ch, 0
    mov si, di
    add si, 2
    xor bx, bx
    mov byte ptr [di + 1], 0     /* clear length up front */
.ah0a_loop:
    call check_break
    cmp bl, cl
    jae .ah0a_wait_cr
    mov ah, 0x00
    int 0x16
    cmp al, 0x0D
    je .ah0a_cr
    cmp al, 0x08
    je .ah0a_bs
    cmp al, 0x20
    jb .ah0a_loop
    mov [si], al
    inc si
    inc bx
    call putch
    jmp .ah0a_loop
.ah0a_bs:
    test bx, bx
    jz .ah0a_loop
    dec bx
    dec si
    mov al, 0x08
    call putch
    mov al, 0x20
    call putch
    mov al, 0x08
    call putch
    jmp .ah0a_loop
.ah0a_wait_cr:
    mov ah, 0x00
    int 0x16
    cmp al, 0x0D
    jne .ah0a_wait_cr
.ah0a_cr:
    mov [di + 1], bl
    mov al, 0x0D
    call putch
    mov al, 0x0A
    call putch
    pop di
    pop si
    pop cx
    pop bx
    clc
    jmp int21_iret
