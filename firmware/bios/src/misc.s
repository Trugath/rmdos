.code16
.intel_syntax noprefix

.include "firmware/bios/inc/equates.inc"

/*
 * rmDOS clean-room XT system BIOS — miscellaneous services:
 * default ISR, equipment/memory, COM1 INT 14h, INT 15h wait/no-ops,
 * printer stub, no-BASIC INT 18h, warm-boot entry, F1 error pause.
 */

.section .text
.global isr_default
.global int11_handler, int12_handler, int14_handler, int15_handler
.global int17_handler, int18_handler
.global int5_handler
.global cad_main, f1_wait

isr_default:
    iret

int11_handler:
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov ax, [BDA_EQUIP]
    pop ds
    iret

int12_handler:
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov ax, [BDA_MEMKB]
    pop ds
    iret

/*
 * INT 14h — serial (COM1 @ BDA_COM1). DX = port index (0 = COM1).
 * AH=00 init AL=params, AH=01 send AL, AH=02 recv → AL, AH=03 status.
 * Timeout → AH bit7 set.
 */
int14_handler:
    sti
    cmp dx, 0
    jne .i14_bad_port
    cmp ah, 0x00
    je .i14_init
    cmp ah, 0x01
    je .i14_send
    cmp ah, 0x02
    je .i14_recv
    cmp ah, 0x03
    je .i14_status
.i14_bad_port:
    mov ah, 0x80
    iret

.i14_init:
    push bx
    push cx
    push dx
    push ds
    push ax                      /* save AL params in low byte */
    mov bx, BDA_SEG
    mov ds, bx
    mov dx, [BDA_COM1]
    test dx, dx
    jz .i14_init_fail

    /* baud index = AL bits 7-5 → divisor from table */
    pop ax
    push ax
    push ds
    push cs
    pop ds
    mov bl, al
    mov cl, 5
    shr bl, cl
    and bx, 7
    shl bx, 1
    mov cx, [uart_divisors + bx]
    pop ds

    /* set DLAB, program divisor */
    add dx, 3                    /* LCR */
    mov al, 0x80
    out dx, al
    sub dx, 3                    /* divisor latch LSB */
    mov al, cl
    out dx, al
    inc dx
    mov al, ch
    out dx, al

    /* LCR from AL: word len / stop / parity (clear DLAB) */
    pop ax
    push ax
    mov bl, al
    and al, 0x1F                 /* keep bits 4-0 for LCR-ish */
    /* map BIOS AL[4:0] → LCR: bits1-0 word, bit2 stop, bits4-3 parity → LCR 3-5 */
    mov al, bl
    and al, 0x03                 /* word length */
    mov ah, bl
    and ah, 0x04                 /* stop bits */
    or al, ah
    mov ah, bl
    and ah, 0x18                 /* parity */
    shl ah, 1                    /* → LCR bits 4-3... BIOS 4-3 → LCR 4-3: shift 0? */
    /* BIOS bits 4-3 are already parity in LCR positions 4-3 if we map:
       LCR: bit3=parity enable, bit4=even. BIOS: 00 none, 01 odd, 11 even.
       odd: LCR=0x08, even: LCR=0x18, none: 0 */
    mov ah, bl
    and ah, 0x18
    cmp ah, 0x08
    je .i14_par_odd
    cmp ah, 0x18
    je .i14_par_even
    jmp .i14_par_done
.i14_par_odd:
    or al, 0x08
    jmp .i14_par_done
.i14_par_even:
    or al, 0x18
.i14_par_done:
    mov dx, [BDA_COM1]
    add dx, 3
    out dx, al                   /* LCR, DLAB clear */

    /* MCR: DTR|RTS|OUT2 */
    inc dx
    mov al, 0x0B
    out dx, al

    /* disable UART interrupts */
    mov dx, [BDA_COM1]
    inc dx
    xor al, al
    out dx, al

    /* clear pending by reading RBR/LSR/MSR */
    dec dx
    in al, dx
    add dx, 5
    in al, dx
    inc dx
    in al, dx

    call uart_read_status        /* AH=LSR, AL=MSR */
    pop bx                       /* discard saved params */
    pop ds
    pop dx
    pop cx
    pop bx
    iret

.i14_init_fail:
    pop ax
    pop ds
    pop dx
    pop cx
    pop bx
    mov ah, 0x80
    xor al, al
    iret

.i14_send:
    push bx
    push cx
    push dx
    push ds
    push ax                      /* AL = char */
    mov bx, BDA_SEG
    mov ds, bx
    mov dx, [BDA_COM1]
    test dx, dx
    jz .i14_send_fail
    add dx, 5                    /* LSR */
    mov cx, 0xFFFF
.i14_send_wait:
    in al, dx
    test al, 0x20                /* THRE */
    jnz .i14_send_go
    loop .i14_send_wait
    mov ah, al
    or ah, 0x80                  /* timeout + last LSR */
    pop bx                       /* discard char */
    jmp .i14_send_done
.i14_send_go:
    mov dx, [BDA_COM1]
    pop ax
    out dx, al
    call uart_read_status
.i14_send_done:
    pop ds
    pop dx
    pop cx
    pop bx
    iret
.i14_send_fail:
    pop ax
    pop ds
    pop dx
    pop cx
    pop bx
    mov ah, 0x80
    iret

.i14_recv:
    push bx
    push cx
    push dx
    push ds
    mov bx, BDA_SEG
    mov ds, bx
    mov dx, [BDA_COM1]
    test dx, dx
    jz .i14_recv_fail
    add dx, 5
    mov cx, 0xFFFF
.i14_recv_wait:
    in al, dx
    test al, 0x01                /* DR */
    jnz .i14_recv_go
    loop .i14_recv_wait
    mov ah, 0x80
    xor al, al
    jmp .i14_recv_done
.i14_recv_go:
    mov dx, [BDA_COM1]
    in al, dx
    push ax
    call uart_read_status        /* destroys AL */
    pop bx
    mov al, bl                   /* received char */
.i14_recv_done:
    pop ds
    pop dx
    pop cx
    pop bx
    iret
.i14_recv_fail:
    pop ds
    pop dx
    pop cx
    pop bx
    mov ah, 0x80
    xor al, al
    iret

.i14_status:
    push dx
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov dx, [BDA_COM1]
    test dx, dx
    jz .i14_st_fail
    call uart_read_status
    pop ds
    pop dx
    iret
.i14_st_fail:
    pop ds
    pop dx
    mov ah, 0x80
    xor al, al
    iret

/* DS = BDA, [BDA_COM1] valid. OUT: AH=LSR, AL=MSR. Clobbers DX. */
uart_read_status:
    mov dx, [BDA_COM1]
    add dx, 5
    in al, dx
    mov ah, al
    inc dx
    in al, dx
    ret

/* Baud divisors for 1.8432 MHz (index = AL bits 7-5). */
/* Baud divisors live at F000:E729 (see bios_entries.s uart_divisors). */

/*
 * INT 15h — XT-safe: AH=86 wait, AH=80/81/82 succeed, else CF.
 */
int15_handler:
    sti
    cmp ah, 0x86
    je .i15_wait
    cmp ah, 0x80
    je .i15_ok
    cmp ah, 0x81
    je .i15_ok
    cmp ah, 0x82
    je .i15_ok
    cmp ah, 0x83
    je .i15_fail
    /* unsupported */
.i15_fail:
    mov ah, 0x86
    push bp
    mov bp, sp
    or word ptr [bp + 6], 0x0001
    pop bp
    iret

.i15_ok:
    push bp
    mov bp, sp
    and word ptr [bp + 6], 0xFFFE
    pop bp
    iret

.i15_wait:
    /* CX:DX = microseconds → wait at least ceil(us/55000) ticks (min 1). */
    push ax
    push bx
    push cx
    push dx
    push ds
    push si
    push di

    mov ax, dx
    mov dx, cx                   /* DX:AX = µs */
    mov bx, 55000
    cmp dx, 0
    jne .i15_div32
    cmp ax, 0
    je .i15_wait_done            /* zero wait */
    xor dx, dx
    div bx                       /* AX = ticks */
    jmp .i15_ticks
.i15_div32:
    /* DX:AX / BX with DX possibly >= BX — reduce high first */
    push ax
    mov ax, dx
    xor dx, dx
    div bx                       /* AX = high/BX, DX = rem */
    mov si, ax                   /* high quotient (usually 0) */
    pop ax
    div bx                       /* DX:AX = rem:low / BX → AX quot */
    add ax, si
.i15_ticks:
    test ax, ax
    jnz .i15_have
    mov ax, 1
.i15_have:
    mov di, ax                   /* ticks to wait */
    mov ax, BDA_SEG
    mov ds, ax
    mov bx, [BDA_TIMER_LO]
    mov cx, [BDA_TIMER_HI]
.i15_spin:
    mov ax, [BDA_TIMER_LO]
    mov dx, [BDA_TIMER_HI]
    sub ax, bx
    sbb dx, cx
    cmp ax, di
    jae .i15_wait_done
    /* also if high differed enough */
    test dx, dx
    jnz .i15_wait_done
    hlt
    jmp .i15_spin

.i15_wait_done:
    pop di
    pop si
    pop ds
    pop dx
    pop cx
    pop bx
    pop ax
    push bp
    mov bp, sp
    and word ptr [bp + 6], 0xFFFE
    pop bp
    iret

/*
 * INT 05h — Print Screen (IBM). Status at 0000:0500:
 *  00 = idle/OK, 01 = in progress, FF = error (e.g. no printer).
 */
int5_handler:
    sti
    push ds
    push ax
    push bx
    push cx
    push dx
    xor ax, ax
    mov ds, ax
    cmp byte ptr [0x0500], 1
    je .i5_done
    mov byte ptr [0x0500], 1

    /* CR+LF before dump */
    call .i5_crlf

    mov ah, 0x0F
    int 0x10                     /* AH=cols, AL=mode */
    push ax
    mov ah, 0x03
    int 0x10                     /* DX = cursor */
    pop ax
    push dx                      /* save cursor */
    mov ch, 25                   /* 25 rows */
    mov cl, ah                   /* columns */
    xor dx, dx

.i5_loop:
    mov ah, 0x02
    int 0x10
    mov ah, 0x08
    int 0x10
    test al, al
    jnz .i5_print
    mov al, ' '
.i5_print:
    push dx
    xor dx, dx
    mov ah, 0                    /* INT 17 print */
    int 0x17
    pop dx
    test ah, 0x25                /* timeout / I/O / out-of-paper */
    jz .i5_next
    mov byte ptr [0x0500], 0xFF
    jmp .i5_restore
.i5_next:
    inc dl
    cmp dl, cl
    jne .i5_loop
    xor dl, dl
    call .i5_crlf
    inc dh
    cmp dh, ch
    jne .i5_loop
    mov byte ptr [0x0500], 0
.i5_restore:
    pop dx
    mov ah, 0x02
    int 0x10
.i5_done:
    pop dx
    pop cx
    pop bx
    pop ax
    pop ds
    iret

.i5_crlf:
    push ax
    push dx
    xor dx, dx
    mov al, 0x0D
    mov ah, 0
    int 0x17
    mov al, 0x0A
    mov ah, 0
    int 0x17
    pop dx
    pop ax
    ret

int17_handler:
    /*
     * INT 17h printer: DX=0 (LPT1) against BDA 40:08 base (0x378).
     * AH=00 write AL, AH=01 init, AH=02 status. DX≠0 → timeout.
     */
    sti
    push bx
    push cx
    push dx
    push ds
    cmp dx, 0
    jne .i17_timeout
    mov bx, BDA_SEG
    mov ds, bx
    mov dx, word ptr [BDA_LPT1]
    test dx, dx
    jz .i17_timeout
    cmp ah, 0
    je .i17_write
    cmp ah, 1
    je .i17_init
    cmp ah, 2
    je .i17_status
    /* unknown: fall through as status */
.i17_status:
    call .i17_read_stat
    jmp .i17_done
.i17_init:
    /* pulse /INIT on control (base+2): bit2 low then high */
    push ax
    mov bx, dx
    add dx, 2
    in al, dx
    and al, 0xFB
    out dx, al
    mov cx, 50
.i17_init_w:
    loop .i17_init_w
    or al, 0x04
    out dx, al
    mov dx, bx
    pop ax
    call .i17_read_stat
    jmp .i17_done
.i17_write:
    /* data latch */
    out dx, al
    /* strobe pulse on control bit 0 */
    push ax
    mov bx, dx
    add dx, 2
    in al, dx
    or al, 0x01
    out dx, al
    mov cx, 10
.i17_str_w:
    loop .i17_str_w
    and al, 0xFE
    out dx, al
    mov dx, bx
    pop ax
    /* Prefer ready status; floating/unmapped ports still report success */
    call .i17_read_stat
    and ah, 0xFE
    jmp .i17_done
.i17_timeout:
    mov ah, 0x01
.i17_done:
    pop ds
    pop dx
    pop cx
    pop bx
    iret

/* DX = data base. Out: AH = classic printer status (prefer selected+ready). */
.i17_read_stat:
    push dx
    push ax
    inc dx
    in al, dx
    mov ah, al
    and ah, 0xF8
    or ah, 0x10                  /* selected */
    and ah, 0xDF                 /* clear paper-out if floating bus */
    pop dx
    mov al, dl
    pop dx
    ret

int18_handler:
    push cs
    pop ds
    mov si, offset no_basic_msg
    mov ah, 0x0E
    mov bh, 0
.i18_loop:
    lodsb
    test al, al
    jz .i18_halt
    int 0x10
    jmp .i18_loop
.i18_halt:
    hlt
    jmp .i18_halt

cad_main:
    /* Kept for callers that jump here directly; CAD vector at EA82 is a far JMP. */
    cli
    mov ax, BDA_SEG
    mov ds, ax
    mov word ptr [BDA_WARM_FLAG], WARM_BOOT_MAGIC
    jmp post_main

f1_wait:
    sti
.f1_loop:
    mov ah, 0x00
    int 0x16
    cmp ah, 0x3B                 /* F1 */
    je .f1_done
    cmp ah, 0x01                 /* Esc also dismisses error pause */
    je .f1_done
    jmp .f1_loop
.f1_done:
    ret

no_basic_msg:
    .asciz "rmDOS: no ROM BASIC\r\n"
