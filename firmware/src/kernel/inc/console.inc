/*
 * Console helpers + INT 21h character I/O (AH=01/02/06/07/08/09/0A/0B/0C).
 * Assumes CS=DS for kernel data; INT 21h uses caller DS for string buffers.
 * putch → current CON device OUTPUT (see device.inc).
 */

/* Write AL via CON driver (1-byte OUTPUT). Preserves AL. */
putch:
    push ax
    push bx
    push cx
    push dx
    push ds
    cmp byte ptr cs:[dev_putch_reent], 0
    jne .putch_direct
    mov byte ptr cs:[dev_putch_reent], 1
    mov byte ptr cs:[putch_byte], al
    push cs
    pop ds
    lea dx, [putch_byte]
    mov cx, 1
    call con_dev_write
    mov byte ptr cs:[dev_putch_reent], 0
    jmp .putch_done
.putch_direct:
    call con_teletype
.putch_done:
    pop ds
    pop dx
    pop cx
    pop bx
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

/* AH=03h — AUX input via INT 14h AH=02 COM1 */
dos_ah03:
    call aux_read_byte
    clc
    jmp int21_iret

/* AH=04h — AUX output via INT 14h AH=01 COM1 */
dos_ah04:
    call aux_write_byte
    mov al, dl
    clc
    jmp int21_iret

/* AH=05h — PRN output via INT 17h AH=00 LPT1 */
dos_ah05:
    call prn_write_byte
    mov al, dl
    clc
    jmp int21_iret

/* Read one AUX byte → AL. COM1 (DX=0). Preserves other regs except AX scratch. */
aux_read_byte:
    push dx
    xor dx, dx
    mov ah, 0x02
    int 0x14
    pop dx
    ret

/* Write DL to AUX (COM1). Preserves DL. */
aux_write_byte:
    push ax
    push dx
    mov al, dl
    xor dx, dx
    mov ah, 0x01
    int 0x14
    pop dx
    pop ax
    ret

/* Write DL to PRN (LPT1). Preserves DL. */
prn_write_byte:
    push ax
    push dx
    mov al, dl
    xor dx, dx
    mov ah, 0x00
    int 0x17
    pop dx
    pop ax
    ret

/* INT 21h AH=09 — write DS:DX '$'-terminated */

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
    push ds
    push cs
    pop ds
    lea dx, [putch_byte]
    mov cx, 1
    call con_dev_read
    pop ds
    mov al, byte ptr cs:[putch_byte]
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
    push ds
    push cs
    pop ds
    lea dx, [putch_byte]
    mov cx, 1
    call con_dev_read
    pop ds
    mov al, byte ptr cs:[putch_byte]
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

/*
 * INT 21h AH=0Ch — flush keyboard buffer, then dispatch AL as
 * console function 01/06/07/08/0A (others: flush only).
 */
dos_ah0c:
    push ax
.a0c_flush:
    mov ah, 0x01
    int 0x16
    jz .a0c_flushed
    mov ah, 0x00
    int 0x16
    jmp .a0c_flush
.a0c_flushed:
    pop ax
    mov ah, al
    cmp ah, 0x01
    je dos_ah01
    cmp ah, 0x06
    je dos_ah06
    cmp ah, 0x07
    je dos_ah07
    cmp ah, 0x08
    je dos_ah08
    cmp ah, 0x0A
    je dos_ah0a
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
    push bx
    call check_break
    pop bx
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
