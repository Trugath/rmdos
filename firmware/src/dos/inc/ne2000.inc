/*
 * NE2000 / DE-220 driver for rmDOS userland COM tools (PING, DHCP).
 *
 * Include after app .set overrides. Defaults:
 *   NE_BASE=0x300, TX_PAGE=0x40, RX_START=0x46, RX_STOP=0x60
 *   NE_RX_POLL_OUTER=0x30  (outer nic_rx timeout loops)
 *
 * Caller must provide near data: my_mac, tx_buf, rx_buf, tx_len, rx_len,
 * poll_left; and near check_abort (ZF clear / NZ = abort).
 * nic_transmit: SI=packet, CX=length. nic_rx: CF=timeout/error, else CX=len.
 */

.ifndef NE_BASE
.set NE_BASE, 0x300
.endif
.ifndef TX_PAGE
.set TX_PAGE, 0x40
.endif
.ifndef RX_START
.set RX_START, 0x46
.endif
.ifndef RX_STOP
.set RX_STOP, 0x60
.endif
.ifndef NE_RX_POLL_OUTER
.set NE_RX_POLL_OUTER, 0x30
.endif

outb_ne:
    push dx
    add dx, NE_BASE
    out dx, al
    pop dx
    ret

inb_ne:
    push dx
    add dx, NE_BASE
    in al, dx
    pop dx
    ret

nic_init:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    mov dx, 0x1F
    xor al, al
    call outb_ne
    mov dx, 0
    mov al, 0x21
    call outb_ne
    mov dx, 0x0E
    mov al, 0x48
    call outb_ne
    mov dx, 0x0A
    xor al, al
    call outb_ne
    mov dx, 0x0B
    call outb_ne
    mov dx, 0x01
    mov al, RX_START
    call outb_ne
    mov dx, 0x02
    mov al, RX_STOP
    call outb_ne
    mov dx, 0x03
    mov al, RX_START
    call outb_ne
    mov dx, 0x07
    mov al, 0xFF
    call outb_ne
    mov dx, 0x0F
    xor al, al
    call outb_ne
    mov dx, 0x0C
    mov al, 0x04
    call outb_ne
    mov dx, 0x0D
    xor al, al
    call outb_ne
    mov dx, 0
    mov al, 0x61
    call outb_ne
    mov dx, 0x07
    mov al, RX_START + 1
    call outb_ne
    mov dx, 0
    mov al, 0x22
    call outb_ne

    mov dx, 0x0A
    mov al, 12
    call outb_ne
    mov dx, 0x0B
    xor al, al
    call outb_ne
    mov dx, 0x08
    xor al, al
    call outb_ne
    mov dx, 0x09
    xor al, al
    call outb_ne
    mov dx, 0
    mov al, 0x0A
    call outb_ne
    lea di, [my_mac]
    mov cx, 6
.ne_prom:
    mov dx, 0x10
    call inb_ne
    stosb
    mov dx, 0x10
    call inb_ne
    loop .ne_prom

    mov dx, 0
    mov al, 0x61
    call outb_ne
    lea si, [my_mac]
    mov bx, 1
.ne_par:
    lodsb
    mov dx, bx
    call outb_ne
    inc bx
    cmp bx, 7
    jb .ne_par
    mov dx, 0
    mov al, 0x22
    call outb_ne
    clc
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

remote_write:
    push ax
    push cx
    push dx
    push si
    mov dx, 0x0A
    mov al, cl
    call outb_ne
    mov dx, 0x0B
    mov al, ch
    call outb_ne
    mov dx, 0x08
    mov al, bl
    call outb_ne
    mov dx, 0x09
    mov al, bh
    call outb_ne
    mov dx, 0
    mov al, 0x12
    call outb_ne
.ne_rw:
    lodsb
    mov dx, 0x10
    call outb_ne
    loop .ne_rw
    mov cx, 0x4000
.ne_rw_rdc:
    mov dx, 0x07
    call inb_ne
    test al, 0x40
    jnz .ne_rw_ok
    loop .ne_rw_rdc
.ne_rw_ok:
    mov dx, 0x07
    mov al, 0x40
    call outb_ne
    pop si
    pop dx
    pop cx
    pop ax
    ret

nic_transmit:
    push ax
    push bx
    push cx
    push dx
    push si
    mov [tx_len], cx
    mov bx, (TX_PAGE << 8)
    call remote_write
    mov dx, 0x04
    mov al, TX_PAGE
    call outb_ne
    mov ax, [tx_len]
    mov dx, 0x05
    call outb_ne
    mov dx, 0x06
    mov al, ah
    call outb_ne
    mov dx, 0
    mov al, 0x26
    call outb_ne
    mov cx, 0x8000
.ne_txw:
    mov dx, 0x07
    call inb_ne
    test al, 0x02
    jnz .ne_txok
    loop .ne_txw
.ne_txok:
    mov dx, 0x07
    mov al, 0x02
    call outb_ne
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

nic_rx:
    push ax
    push bx
    push dx
    push si
    push di
    mov word ptr [poll_left], NE_RX_POLL_OUTER
    jmp .ne_rx_poll

/* Same as nic_rx but only one outer poll — for drain loops. */
nic_rx_quick:
    push ax
    push bx
    push dx
    push si
    push di
    mov word ptr [poll_left], 1
.ne_rx_poll:
    call check_abort
    jnz .ne_rx_to
    mov cx, 0x1000
.ne_rx_spin:
    /* Ring may hold packets after ISR.PRX was cleared — check CURR vs BNRY
     * every 256 polls (when CL wraps) and on the first iteration. */
    test cl, 0xFF
    jnz .ne_rx_isr
    call ne_rx_pending
    jnz .ne_rx_got
    call check_abort
    jnz .ne_rx_to
.ne_rx_isr:
    mov dx, 0x07
    call inb_ne
    test al, 0x01
    jnz .ne_rx_got
.ne_rx_spin_cont:
    loop .ne_rx_spin
    dec word ptr [poll_left]
    jnz .ne_rx_poll
.ne_rx_to:
    stc
    jmp .ne_rx_ret

/* NZ if BNRY+1 != CURR (unread frame in RX ring). Clobbers AX/DX; saves page. */
ne_rx_pending:
    push bx
    mov dx, 0
    mov al, 0x62                       /* page 1 + STA */
    call outb_ne
    mov dx, 0x07
    call inb_ne                        /* CURR */
    mov bh, al
    mov dx, 0
    mov al, 0x22                       /* page 0 + STA */
    call outb_ne
    mov dx, 0x03
    call inb_ne                        /* BNRY */
    inc al
    cmp al, RX_STOP
    jb .ne_rp_wrap
    mov al, RX_START
.ne_rp_wrap:
    cmp al, bh
    pop bx
    ret                                /* ZF=empty, NZ=pending */

.ne_rx_got:
    mov dx, 0x07
    mov al, 0x01
    call outb_ne
    mov dx, 0x03
    call inb_ne
    mov bl, al
    inc bl
    cmp bl, RX_STOP
    jb .ne_rx_pg
    mov bl, RX_START
.ne_rx_pg:
    mov dx, 0x0A
    mov al, 4
    call outb_ne
    mov dx, 0x0B
    xor al, al
    call outb_ne
    mov dx, 0x08
    xor al, al
    call outb_ne
    mov dx, 0x09
    mov al, bl
    call outb_ne
    mov dx, 0
    mov al, 0x0A
    call outb_ne
    mov dx, 0x10
    call inb_ne
    mov dx, 0x10
    call inb_ne
    mov bh, al
    mov dx, 0x10
    call inb_ne
    mov cl, al
    mov dx, 0x10
    call inb_ne
    mov ch, al
    cmp cx, 4
    jbe .ne_rx_skip
    sub cx, 4
    cmp cx, 1514
    ja .ne_rx_skip
    mov [rx_len], cx
    mov dx, 0x0A
    mov al, cl
    call outb_ne
    mov dx, 0x0B
    mov al, ch
    call outb_ne
    mov dx, 0x08
    mov al, 4
    call outb_ne
    mov dx, 0x09
    mov al, bl
    call outb_ne
    mov dx, 0
    mov al, 0x0A
    call outb_ne
    lea di, [rx_buf]
    mov cx, [rx_len]
.ne_rx_cp:
    mov dx, 0x10
    call inb_ne
    stosb
    loop .ne_rx_cp
    call ne_rx_advance_bnry
    mov cx, [rx_len]
    clc
    jmp .ne_rx_ret
.ne_rx_skip:
    /* Drop bogus descriptor so the ring cannot wedge with ISR clear. */
    call ne_rx_advance_bnry
    stc
.ne_rx_ret:
    pop di
    pop si
    pop dx
    pop bx
    pop ax
    ret

/* BH = next packet page from RX header. Sets BNRY = BH-1 (with wrap). */
ne_rx_advance_bnry:
    push ax
    push dx
    mov al, bh
    dec al
    cmp al, RX_START - 1
    ja .ne_ab_ok
    mov al, RX_STOP - 1
    jmp .ne_ab_set
.ne_ab_ok:
    cmp al, RX_START
    jae .ne_ab_set
    mov al, RX_STOP - 1
.ne_ab_set:
    mov dx, 0x03
    call outb_ne
    pop dx
    pop ax
    ret

inet_csum:
    push bx
    push cx
    push dx
    push si
    xor bx, bx
    xor dx, dx
.ne_cs1:
    cmp cx, 2
    jb .ne_cs_odd
    lodsb
    mov dh, al
    lodsb
    mov dl, al
    add bx, dx
    adc bx, 0
    sub cx, 2
    jmp .ne_cs1
.ne_cs_odd:
    test cx, cx
    jz .ne_cs_fold
    lodsb
    mov dh, al
    xor dl, dl
    add bx, dx
    adc bx, 0
.ne_cs_fold:
    mov ax, bx
    mov dx, ax
    mov cl, 16
    shr dx, cl
    add ax, dx
    mov dx, ax
    shr dx, cl
    add ax, dx
    not ax
    pop si
    pop dx
    pop cx
    pop bx
    ret
