.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * PING.COM — Windows-like ICMP echo via DE-220 NE2000 @ 0x300.
 * Source IP / gateway come from LEASE.DAT (written by DHCP.COM).
 * Usage: PING [-t] [-n count] ip
 */

.set NE_BASE, 0x300
.set TX_PAGE, 0x40
.set RX_START, 0x46
.set RX_STOP, 0x60
.set PAYLOAD, 32
.set ICMP_LEN, (8 + PAYLOAD)
.set IP_TOTAL, (20 + ICMP_LEN)
.set DEF_COUNT, 4
.set ICMP_ID, 0x1234
.set LEASE_SIZE, 24

_start:
    push cs
    pop ds
    push cs
    pop es

    call parse_args
    cmp byte ptr [args_err], 0
    jne .show_help
    cmp byte ptr [target_set], 0
    jne .have_target
.show_help:
    mov ah, 0x09
    lea dx, [msg_help]
    int 0x21
    mov ax, 0x4C01
    int 0x21

.have_target:
    call load_lease
    jc .no_lease

    call install_break

    call nic_init
    jc .hard_fail
    /* Always ARP the default gateway; off-subnet targets are routed via it. */
    call do_arp_gateway
    jc .hard_fail

    call print_banner
    call ping_loop
    call print_stats

    cmp byte ptr [abort_flag], 0
    je .no_break_msg
    mov ah, 0x09
    lea dx, [msg_break]
    int 0x21
.no_break_msg:
    cmp word ptr [recv_count], 0
    je .exit_fail
    mov ax, 0x4C00
    int 0x21

.no_lease:
    mov ah, 0x09
    lea dx, [msg_no_lease]
    int 0x21
    jmp .exit_fail

.hard_fail:
    mov ah, 0x09
    lea dx, [msg_unreach]
    int 0x21
.exit_fail:
    mov ax, 0x4C01
    int 0x21

/* Read A:\LEASE.DAT written by DHCP.COM.
 * Layout: "DHCP" + ver + pad3 + yiaddr4 + gw4 + mask4 + dns4 (24 bytes). */
load_lease:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov ah, 0x3D
    xor al, al
    lea dx, [lease_path]
    int 0x21
    jc .ll_fail
    mov [lease_handle], ax

    mov ah, 0x3F
    mov bx, [lease_handle]
    mov cx, LEASE_SIZE
    lea dx, [lease_buf]
    int 0x21
    jc .ll_close_fail
    cmp ax, LEASE_SIZE
    jne .ll_close_fail

    mov ah, 0x3E
    mov bx, [lease_handle]
    int 0x21

    cmp word ptr [lease_buf], 0x4844     /* 'DH' */
    jne .ll_fail
    cmp word ptr [lease_buf + 2], 0x5043 /* 'CP' */
    jne .ll_fail
    cmp byte ptr [lease_buf + 4], 1
    jne .ll_fail

    lea si, [lease_buf + 8]
    lea di, [my_ip]
    movsw
    movsw
    lea di, [gateway_ip]
    movsw
    movsw
    /* require non-zero host IP */
    mov ax, [my_ip]
    or ax, [my_ip + 2]
    jz .ll_fail
    mov ax, [gateway_ip]
    or ax, [gateway_ip + 2]
    jz .ll_fail
    clc
    jmp .ll_done

.ll_close_fail:
    mov ah, 0x3E
    mov bx, [lease_handle]
    int 0x21
.ll_fail:
    stc
.ll_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

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
    cmp byte ptr [abort_flag], 0
    jne .ca_yes
    push ax
    push bx
    push ds
    mov ah, 0x01
    int 0x16
    jz .ca_dos
    cmp al, 0x03
    je .ca_take
    mov bl, al
    mov ax, 0x40
    mov ds, ax
    test byte ptr [0x17], 0x04
    jz .ca_dos_rest
    push cs
    pop ds
    or bl, 0x20
    cmp bl, 'c'
    jne .ca_dos
.ca_take:
    push cs
    pop ds
    mov ah, 0x00
    int 0x16
    mov byte ptr [abort_flag], 1
    jmp .ca_no
.ca_dos_rest:
    push cs
    pop ds
.ca_dos:
    mov ah, 0x0B
    int 0x21
.ca_no:
    pop ds
    pop bx
    pop ax
    cmp byte ptr [abort_flag], 0
    ret
.ca_yes:
    ret

get_ticks:
    /* AX = low BIOS tick word */
    push ds
    mov ax, 0x40
    mov ds, ax
    mov ax, [0x6C]
    pop ds
    ret

/* Wait CX BIOS ticks (~55ms each), abortable. */
wait_ticks:
    push ax
    push bx
    push cx
    push dx
    call get_ticks
    mov bx, ax
.wt:
    call check_abort
    jnz .wt_done
    call get_ticks
    mov dx, ax
    sub dx, bx
    cmp dx, cx
    jb .wt
.wt_done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

print_banner:
    push ax
    push dx
    mov ah, 0x09
    lea dx, [msg_pinging]
    int 0x21
    lea si, [target_ip]
    call print_ip
    mov ah, 0x09
    lea dx, [msg_with]
    int 0x21
    pop dx
    pop ax
    ret

ping_loop:
    push ax
    push bx
    push cx
    mov word ptr [seq_num], 1
.pl_next:
    call check_abort
    jnz .pl_done
    cmp byte ptr [continuous], 0
    jne .pl_send
    mov ax, [seq_num]
    cmp ax, [ping_count]
    ja .pl_done

.pl_send:
    inc word ptr [sent_count]
    call do_icmp_once
    jc .pl_timeout

    /* success: AX=rtt_ms, BL=ttl */
    inc word ptr [recv_count]
    call update_rtt_stats
    call print_reply
    jmp .pl_gap

.pl_timeout:
    mov ah, 0x09
    lea dx, [msg_timeout]
    int 0x21

.pl_gap:
    cmp byte ptr [continuous], 0
    jne .pl_cont
    mov ax, [seq_num]
    cmp ax, [ping_count]
    jae .pl_done
.pl_cont:
    inc word ptr [seq_num]
    /* ~1s between probes */
    mov cx, 18
    call wait_ticks
    jmp .pl_next

.pl_done:
    pop cx
    pop bx
    pop ax
    ret

/* AX = rtt ms, BL = ttl */
update_rtt_stats:
    push ax
    push bx
    cmp word ptr [recv_count], 1
    jne .urs_minmax
    mov [rtt_min], ax
    mov [rtt_max], ax
    mov [rtt_sum], ax
    jmp .urs_done
.urs_minmax:
    add [rtt_sum], ax
    cmp ax, [rtt_min]
    jae .urs_max
    mov [rtt_min], ax
.urs_max:
    cmp ax, [rtt_max]
    jbe .urs_done
    mov [rtt_max], ax
.urs_done:
    mov [last_ttl], bl
    mov [last_rtt], ax
    pop bx
    pop ax
    ret

print_reply:
    push ax
    push dx
    push si
    mov ah, 0x09
    lea dx, [msg_reply]
    int 0x21
    lea si, [target_ip]
    call print_ip
    mov ah, 0x09
    lea dx, [msg_bytes]
    int 0x21
    mov ax, [last_rtt]
    test ax, ax
    jnz .pr_time
    mov ah, 0x09
    lea dx, [msg_time_lt]
    int 0x21
    jmp .pr_ttl
.pr_time:
    mov ah, 0x09
    lea dx, [msg_time_eq]
    int 0x21
    mov ax, [last_rtt]
    call print_u16
    mov ah, 0x09
    lea dx, [msg_ms]
    int 0x21
.pr_ttl:
    mov ah, 0x09
    lea dx, [msg_ttl]
    int 0x21
    xor ax, ax
    mov al, [last_ttl]
    call print_u16
    mov ah, 0x09
    lea dx, [msg_crlf]
    int 0x21
    pop si
    pop dx
    pop ax
    ret

print_stats:
    push ax
    push bx
    push cx
    push dx
    mov ah, 0x09
    lea dx, [msg_stats_hdr]
    int 0x21
    lea si, [target_ip]
    call print_ip
    mov ah, 0x09
    lea dx, [msg_stats_1]
    int 0x21
    mov ax, [sent_count]
    call print_u16
    mov ah, 0x09
    lea dx, [msg_stats_2]
    int 0x21
    mov ax, [recv_count]
    call print_u16
    mov ah, 0x09
    lea dx, [msg_stats_3]
    int 0x21
    /* lost = sent - recv */
    mov ax, [sent_count]
    sub ax, [recv_count]
    push ax
    call print_u16
    mov ah, 0x09
    lea dx, [msg_stats_4]
    int 0x21
    pop ax
    /* loss% = lost * 100 / sent */
    mov bx, [sent_count]
    test bx, bx
    jz .ps_rtt
    mov cx, 100
    mul cx
    div bx
    call print_u16
    mov ah, 0x09
    lea dx, [msg_stats_5]
    int 0x21

.ps_rtt:
    cmp word ptr [recv_count], 0
    je .ps_done
    mov ah, 0x09
    lea dx, [msg_rtt_hdr]
    int 0x21
    mov ax, [rtt_min]
    call print_u16
    mov ah, 0x09
    lea dx, [msg_rtt_1]
    int 0x21
    mov ax, [rtt_max]
    call print_u16
    mov ah, 0x09
    lea dx, [msg_rtt_2]
    int 0x21
    mov ax, [rtt_sum]
    xor dx, dx
    mov bx, [recv_count]
    div bx
    call print_u16
    mov ah, 0x09
    lea dx, [msg_rtt_3]
    int 0x21
.ps_done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* print ASCIZ? no — print 4 bytes at SI as dotted decimal */
print_ip:
    push ax
    push bx
    push cx
    push dx
    push si
    mov cx, 4
.pi:
    xor ax, ax
    lodsb
    call print_u16
    dec cx
    jz .pi_done
    mov ah, 0x02
    mov dl, '.'
    int 0x21
    jmp .pi
.pi_done:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* print AX as unsigned decimal */
print_u16:
    push ax
    push bx
    push cx
    push dx
    mov bx, 10
    xor cx, cx
    test ax, ax
    jnz .pu_div
    mov ah, 0x02
    mov dl, '0'
    int 0x21
    jmp .pu_done
.pu_div:
    xor dx, dx
    div bx
    push dx
    inc cx
    test ax, ax
    jnz .pu_div
.pu_out:
    pop dx
    add dl, '0'
    mov ah, 0x02
    int 0x21
    loop .pu_out
.pu_done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* ---- ICMP one shot: CF=timeout; else AX=ms, BL=ttl ---- */
do_icmp_once:
    push bx
    push cx
    push dx
    push si
    push di

    lea di, [tx_buf]
    lea si, [gw_mac]
    mov cx, 3
    rep movsw
    lea si, [my_mac]
    mov cx, 3
    rep movsw
    mov ax, 0x0008
    stosw

    mov al, 0x45
    stosb
    xor al, al
    stosb
    mov ax, IP_TOTAL
    xchg al, ah
    stosw
    mov ax, [seq_num]
    xchg al, ah
    stosw                       /* ident = seq for variety */
    xor ax, ax
    stosw
    mov al, 64
    stosb
    mov al, 1
    stosb
    xor ax, ax
    stosw
    lea si, [my_ip]
    movsw
    movsw
    lea si, [target_ip]
    movsw
    movsw

    /* ICMP */
    mov al, 8
    stosb
    xor al, al
    stosb
    xor ax, ax
    stosw
    mov ax, ICMP_ID
    xchg al, ah
    stosw
    mov ax, [seq_num]
    xchg al, ah
    stosw
    /* payload pattern */
    mov cx, PAYLOAD
    mov al, 'a'
.icmp_pay:
    stosb
    inc al
    cmp al, 'z' + 1
    jb .icmp_pay_ok
    mov al, 'a'
.icmp_pay_ok:
    loop .icmp_pay

    lea si, [tx_buf + 14]
    mov cx, 20
    call inet_csum
    mov [tx_buf + 24], ah
    mov [tx_buf + 25], al

    lea si, [tx_buf + 34]
    mov cx, ICMP_LEN
    call inet_csum
    mov [tx_buf + 36], ah
    mov [tx_buf + 37], al

    mov cx, 14 + IP_TOTAL
    cmp cx, 60
    jae .icmp_tx
    lea di, [tx_buf]
    add di, cx
    mov ax, 60
    sub ax, cx
    mov cx, ax
    xor al, al
    rep stosb
    mov cx, 60
.icmp_tx:
    call get_ticks
    mov [t0], ax
    lea si, [tx_buf]
    call nic_transmit

    mov word ptr [pkt_tries], 6
.icmp_wait:
    call check_abort
    jnz .icmp_fail
    call nic_rx
    jc .icmp_fail
    cmp word ptr [rx_buf + 12], 0x0008
    jne .icmp_next
    cmp byte ptr [rx_buf + 23], 1
    jne .icmp_next
    mov bl, [rx_buf + 14]
    and bx, 0x000F
    shl bx, 1
    shl bx, 1
    lea si, [rx_buf + 14]
    add si, bx
    cmp byte ptr [si], 0
    jne .icmp_next
    cmp word ptr [si + 4], 0x3412
    jne .icmp_next
    mov ax, [seq_num]
    xchg al, ah
    cmp [si + 6], ax
    jne .icmp_next

    call get_ticks
    mov bx, ax
    sub bx, [t0]
    /* ms ≈ ticks * 55 */
    mov ax, bx
    mov cx, 55
    mul cx
    /* TTL */
    mov bl, [rx_buf + 14 + 8]
    clc
    jmp .icmp_done

.icmp_next:
    dec word ptr [pkt_tries]
    jnz .icmp_wait
.icmp_fail:
    stc
.icmp_done:
    pop di
    pop si
    pop dx
    pop cx
    /* BX has ttl in BL on success; don't pop bx yet — need BL */
    /* stack: saved bx */
    mov [tmp_ttl], bl
    pop bx
    mov bl, [tmp_ttl]
    ret

/* ---- ARP gateway (L2 next hop for all traffic on this NAT net) ---- */
do_arp_gateway:
    push ax
    push bx
    push cx
    push si
    push di

    lea di, [tx_buf]
    mov ax, 0xFFFF
    stosw
    stosw
    stosw
    lea si, [my_mac]
    mov cx, 3
    rep movsw
    mov ax, 0x0608
    stosw
    mov ax, 0x0100
    stosw
    mov ax, 0x0008
    stosw
    mov ax, 0x0406
    stosw
    mov ax, 0x0100
    stosw
    lea si, [my_mac]
    mov cx, 3
    rep movsw
    lea si, [my_ip]
    movsw
    movsw
    xor ax, ax
    stosw
    stosw
    stosw
    lea si, [gateway_ip]
    movsw
    movsw
    lea ax, [tx_buf]
    mov cx, di
    sub cx, ax
    cmp cx, 60
    jae .arp_send
    mov ax, 60
    sub ax, cx
    mov cx, ax
    xor al, al
    rep stosb
.arp_send:
    lea si, [tx_buf]
    mov cx, 60
    call nic_transmit

    mov word ptr [pkt_tries], 8
.arp_loop:
    call check_abort
    jnz .arp_fail
    call nic_rx
    jc .arp_fail
    cmp word ptr [rx_buf + 12], 0x0608
    jne .arp_next
    cmp word ptr [rx_buf + 20], 0x0200
    jne .arp_next
    mov ax, [rx_buf + 28]
    cmp ax, [gateway_ip]
    jne .arp_next
    mov ax, [rx_buf + 30]
    cmp ax, [gateway_ip + 2]
    jne .arp_next
    lea si, [rx_buf + 22]
    lea di, [gw_mac]
    mov cx, 3
    rep movsw
    clc
    jmp .arp_done
.arp_next:
    dec word ptr [pkt_tries]
    jnz .arp_loop
.arp_fail:
    stc
.arp_done:
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    ret

/* ---- NE2000 ---- */
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
.prom:
    mov dx, 0x10
    call inb_ne
    stosb
    mov dx, 0x10
    call inb_ne
    loop .prom

    mov dx, 0
    mov al, 0x61
    call outb_ne
    lea si, [my_mac]
    mov bx, 1
.par:
    lodsb
    mov dx, bx
    call outb_ne
    inc bx
    cmp bx, 7
    jb .par
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
.rw:
    lodsb
    mov dx, 0x10
    call outb_ne
    loop .rw
    mov cx, 0x4000
.rw_rdc:
    mov dx, 0x07
    call inb_ne
    test al, 0x40
    jnz .rw_ok
    loop .rw_rdc
.rw_ok:
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
.txw:
    mov dx, 0x07
    call inb_ne
    test al, 0x02
    jnz .txok
    loop .txw
.txok:
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
    mov word ptr [poll_left], 0x20
.rx_poll:
    call check_abort
    jnz .rx_to
    mov cx, 0x1000
.rx_spin:
    mov dx, 0x07
    call inb_ne
    test al, 0x01
    jnz .rx_got
    /* Check Ctrl+C every 256 polls so IRQs can land */
    test cl, 0xFF
    jnz .rx_spin_cont
    call check_abort
    jnz .rx_to
.rx_spin_cont:
    loop .rx_spin
    dec word ptr [poll_left]
    jnz .rx_poll
.rx_to:
    stc
    jmp .rx_ret

.rx_got:
    mov dx, 0x07
    mov al, 0x01
    call outb_ne
    mov dx, 0x03
    call inb_ne
    mov bl, al
    inc bl
    cmp bl, RX_STOP
    jb .rx_pg
    mov bl, RX_START
.rx_pg:
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
    jbe .rx_bad
    sub cx, 4
    cmp cx, 1514
    ja .rx_bad
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
.rx_cp:
    mov dx, 0x10
    call inb_ne
    stosb
    loop .rx_cp
    mov al, bh
    dec al
    cmp al, RX_START - 1
    ja .bn_ok
    mov al, RX_STOP - 1
    jmp .bn_set
.bn_ok:
    cmp al, RX_START
    jae .bn_set
    mov al, RX_STOP - 1
.bn_set:
    mov dx, 0x03
    call outb_ne
    mov cx, [rx_len]
    clc
    jmp .rx_ret
.rx_bad:
    stc
.rx_ret:
    pop di
    pop si
    pop dx
    pop bx
    pop ax
    ret

inet_csum:
    push bx
    push cx
    push dx
    push si
    xor bx, bx
    xor dx, dx
.cs1:
    cmp cx, 2
    jb .cs_odd
    lodsb
    mov dh, al
    lodsb
    mov dl, al
    add bx, dx
    adc bx, 0
    sub cx, 2
    jmp .cs1
.cs_odd:
    test cx, cx
    jz .cs_fold
    lodsb
    mov dh, al
    xor dl, dl
    add bx, dx
    adc bx, 0
.cs_fold:
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

/* Parse [-t] [-n N] destination from PSP tail.
 * Sets target_set on a full IPv4; args_err on any bad token. */
parse_args:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov word ptr [ping_count], DEF_COUNT
    mov byte ptr [continuous], 0
    mov byte ptr [target_set], 0
    mov byte ptr [args_err], 0
    mov byte ptr [target_ip], 0
    mov byte ptr [target_ip + 1], 0
    mov byte ptr [target_ip + 2], 0
    mov byte ptr [target_ip + 3], 0

    mov si, 0x80
    xor cx, cx
    mov cl, [si]
    test cl, cl
    jz .pa_done
    inc si

.pa_tok:
    call skip_spaces
    test cx, cx
    jnz .pa_have
    jmp .pa_done
.pa_have:
    cmp byte ptr [si], '-'
    je .pa_opt
    jmp .pa_ip
.pa_opt:
    inc si
    dec cx
    test cx, cx
    jnz .pa_opt2
    mov byte ptr [args_err], 1
    jmp .pa_done
.pa_opt2:
    lodsb
    dec cx
    or al, 0x20
    cmp al, 't'
    je .pa_t
    cmp al, 'n'
    je .pa_n
    /* unknown switch */
    mov byte ptr [args_err], 1
    jmp .pa_done
.pa_t:
    mov byte ptr [continuous], 1
    jmp .pa_tok
.pa_n:
    call skip_spaces
    test cx, cx
    jnz .pa_n2
    mov byte ptr [args_err], 1
    jmp .pa_done
.pa_n2:
    /* require a digit */
    mov bl, [si]
    cmp bl, '0'
    jb .pa_n_bad
    cmp bl, '9'
    ja .pa_n_bad
    call parse_num
    test ax, ax
    jz .pa_n_bad
    cmp ax, 100
    jbe .pa_count_ok
    mov ax, 100
.pa_count_ok:
    mov [ping_count], ax
    jmp .pa_tok
.pa_n_bad:
    mov byte ptr [args_err], 1
    jmp .pa_done

.pa_ip:
    /* only one destination allowed */
    cmp byte ptr [target_set], 0
    je .pa_ip_go
    mov byte ptr [args_err], 1
    jmp .pa_done
.pa_ip_go:
    lea di, [target_ip]
    mov byte ptr [di], 0
    mov byte ptr [di + 1], 0
    mov byte ptr [di + 2], 0
    mov byte ptr [di + 3], 0
    mov bl, 4
.pa_oct:
    /* need at least one digit */
    test cx, cx
    jz .pa_ip_bad
    mov dl, [si]
    cmp dl, '0'
    jb .pa_ip_bad
    cmp dl, '9'
    ja .pa_ip_bad
    xor ax, ax
.pa_dig:
    test cx, cx
    jz .pa_term
    mov dl, [si]
    cmp dl, '0'
    jb .pa_term
    cmp dl, '9'
    ja .pa_term
    lodsb
    dec cx
    sub al, '0'
    mov ah, 0
    push bx
    mov bx, ax
    mov al, [di]
    mov ah, 0
    mov dh, 10
    mul dh
    add ax, bx
    pop bx
    cmp ax, 255
    ja .pa_ip_bad
    mov [di], al
    jmp .pa_dig
.pa_term:
    inc di
    dec bl
    jz .pa_ip_done
    test cx, cx
    jz .pa_ip_bad
    lodsb
    dec cx
    cmp al, '.'
    je .pa_oct
    /* expected dot between octets */
.pa_ip_bad:
    mov byte ptr [args_err], 1
    jmp .pa_done
.pa_ip_done:
    mov byte ptr [target_set], 1
    jmp .pa_tok

.pa_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

skip_spaces:
.ss:
    jcxz .ss_d
    cmp byte ptr [si], ' '
    je .ss_s
    cmp byte ptr [si], 9
    jne .ss_d
.ss_s:
    inc si
    dec cx
    jmp .ss
.ss_d:
    ret

parse_num:
    xor ax, ax
.pn:
    jcxz .pn_d
    mov bl, [si]
    cmp bl, '0'
    jb .pn_d
    cmp bl, '9'
    ja .pn_d
    mov dl, [si]
    inc si
    dec cx
    sub dl, '0'
    xor dh, dh
    push dx
    mov bx, 10
    mul bx
    pop dx
    add ax, dx
    jmp .pn
.pn_d:
    ret

msg_help:
    .ascii "Usage: PING [-t] [-n count] destination\r\n"
    .ascii "\r\n"
    .ascii "Requires a DHCP lease (run DHCP first; reads LEASE.DAT).\r\n"
    .ascii "\r\n"
    .ascii "Options:\r\n"
    .ascii "    -t             Ping the specified host until stopped.\r\n"
    .ascii "    -n count        Number of echo requests to send (default 4).\r\n"
    .ascii "\r\n"
    .ascii "destination         IPv4 address (e.g. 10.0.2.2).\r\n$"
msg_pinging:
    .ascii "\r\nPinging $"
msg_with:
    .ascii " with 32 bytes of data:\r\n\r\n$"
msg_reply:
    .ascii "Reply from $"
msg_bytes:
    .ascii ": bytes=32 $"
msg_time_lt:
    .ascii "time<55ms $"
msg_time_eq:
    .ascii "time=$"
msg_ms:
    .ascii "ms $"
msg_ttl:
    .ascii "TTL=$"
msg_timeout:
    .ascii "Request timed out.\r\n$"
msg_stats_hdr:
    .ascii "\r\nPing statistics for $"
msg_stats_1:
    .ascii ":\r\n    Packets: Sent = $"
msg_stats_2:
    .ascii ", Received = $"
msg_stats_3:
    .ascii ", Lost = $"
msg_stats_4:
    .ascii " ($"
msg_stats_5:
    .ascii "% loss),\r\n$"
msg_rtt_hdr:
    .ascii "Approximate round trip times in milli-seconds:\r\n"
    .ascii "    Minimum = $"
msg_rtt_1:
    .ascii "ms, Maximum = $"
msg_rtt_2:
    .ascii "ms, Average = $"
msg_rtt_3:
    .ascii "ms\r\n$"
msg_crlf:
    .ascii "\r\n$"
msg_unreach:
    .ascii "PING: host unreachable\r\n$"
msg_no_lease:
    .ascii "PING: no DHCP lease. Run DHCP first.\r\n$"
msg_break:
    .ascii "^C\r\n$"

gateway_ip:
    .byte 0, 0, 0, 0
my_ip:
    .byte 0, 0, 0, 0
target_ip:
    .byte 0, 0, 0, 0
target_set:
    .byte 0
args_err:
    .byte 0
lease_path:
    .asciz "LEASE.DAT"
lease_handle:
    .word 0
lease_buf:
    .space LEASE_SIZE, 0
my_mac:
    .space 6, 0
gw_mac:
    .space 6, 0

ping_count:
    .word DEF_COUNT
seq_num:
    .word 1
sent_count:
    .word 0
recv_count:
    .word 0
rtt_min:
    .word 0
rtt_max:
    .word 0
rtt_sum:
    .word 0
last_rtt:
    .word 0
last_ttl:
    .byte 0
tmp_ttl:
    .byte 0
continuous:
    .byte 0
abort_flag:
    .byte 0
t0:
    .word 0
poll_left:
    .word 0
pkt_tries:
    .word 0
tx_len:
    .word 0
rx_len:
    .word 0
tx_buf:
    .space 1600, 0
rx_buf:
    .space 1600, 0
