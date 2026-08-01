.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * TELNET.COM — minimal outbound Telnet client via DE-220 NE2000 @ 0x300.
 * Requires LEASE.DAT (DHCP). Usage: TELNET host [port]
 * host may be IPv4 or DNS hostname. Default port 23. Ctrl-C aborts.
 * NVT: skip IAC WILL/WONT/DO/DONT.
 */

.set LEASE_SIZE, 24
.set NE_RX_POLL_OUTER, 0x04
.set DEF_PORT, 23
.set TCP_HDR, 20
.set IP_HDR, 20
.set ETH_HDR, 14
.set ST_CLOSED, 0
.set ST_SYN_SENT, 1
.set ST_ESTAB, 2
.set ST_CLOSE_WAIT, 3

.include "firmware/src/dos/inc/netlease_defs.inc"

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
    cmp byte ptr [hostname_set], 0
    jne .have_target
.show_help:
    mov ah, 0x09
    lea dx, [msg_help]
    int 0x21
    mov ax, 0x4C01
    int 0x21

.have_target:
    call net_probe
    call load_lease
    jc .no_lease

    call install_break
    call nic_init
    jc .nic_fail
    call do_arp_gateway
    jc .fail_arp

    cmp byte ptr [hostname_set], 0
    je .tn_ready
    mov ah, 0x09
    lea dx, [msg_resolving]
    int 0x21
    lea si, [hostname]
    call dns_resolve
    jc .dns_fail
    mov byte ptr [target_set], 1
.tn_ready:
    call tcp_connect
    jc .conn_fail

    mov ah, 0x09
    lea dx, [msg_connected]
    int 0x21

    call session_loop

    cmp byte ptr [abort_flag], 0
    je .exit_ok
    mov ah, 0x09
    lea dx, [msg_break]
    int 0x21
.exit_ok:
    mov ax, 0x4C00
    int 0x21

.no_lease:
    mov ah, 0x09
    lea dx, [msg_no_lease]
    int 0x21
    jmp .exit_fail
.nic_fail:
    mov ah, 0x09
    lea dx, [msg_nic]
    int 0x21
    jmp .exit_fail
.fail_arp:
    mov ah, 0x09
    lea dx, [msg_arp]
    int 0x21
    jmp .exit_fail
.dns_fail:
    mov ah, 0x09
    lea dx, [msg_dns_fail]
    int 0x21
    jmp .exit_fail
.conn_fail:
    mov ah, 0x09
    lea dx, [msg_conn]
    int 0x21
.exit_fail:
    mov ax, 0x4C01
    int 0x21

load_lease:
    push ax
    push si
    push di
    call net_load_lease
    jc .ll_fail
    lea si, [lease_buf + LEASE_OFF_YIADDR]
    lea di, [my_ip]
    movsw
    movsw
    lea di, [gateway_ip]
    movsw
    movsw
    add si, 4                        /* skip mask */
    lea di, [dns_ip]
    movsw
    movsw
    mov ax, [my_ip]
    or ax, [my_ip + 2]
    jz .ll_fail
    mov ax, [gateway_ip]
    or ax, [gateway_ip + 2]
    jz .ll_fail
    clc
    jmp .ll_done
.ll_fail:
    stc
.ll_done:
    pop di
    pop si
    pop ax
    ret

/* ---- ARP gateway ---- */
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
    mov word ptr [pkt_tries], 10
.arp_loop:
    call check_abort
    jnz .arp_fail
    call nic_rx
    jc .arp_to
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
.arp_to:
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

/* ---- TCP connect (SYN seq=0) ---- */
tcp_connect:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    call get_ticks
    or ax, 0xC000
    mov [local_port], ax
    mov word ptr [snd_nxt], 0
    mov word ptr [snd_nxt + 2], 0
    mov word ptr [rcv_nxt], 0
    mov word ptr [rcv_nxt + 2], 0
    mov word ptr [ip_ident], 1
    mov byte ptr [tcp_state], ST_SYN_SENT

    mov word ptr [tcp_flags], 0x02       /* SYN */
    mov word ptr [tcp_payload_len], 0
    call tcp_send_segment

    mov word ptr [pkt_tries], 80
.tc_wait:
    call check_abort
    jnz .tc_fail
    /* retransmit SYN every 16 polls after the first */
    mov ax, [pkt_tries]
    cmp ax, 80
    je .tc_rx
    and ax, 0x0F
    jnz .tc_rx
    cmp byte ptr [tcp_state], ST_SYN_SENT
    jne .tc_rx
    mov word ptr [snd_nxt], 0
    mov word ptr [snd_nxt + 2], 0
    mov word ptr [tcp_flags], 0x02
    mov word ptr [tcp_payload_len], 0
    call tcp_send_segment
.tc_rx:
    call nic_rx
    jc .tc_to
    call tcp_is_ours
    jc .tc_next
    /* flags at tcp+13 */
    lea bx, [rx_buf]
    add bx, [tcp_off]
    mov al, [bx + 13]
    test al, 0x04                        /* RST */
    jnz .tc_fail
    and al, 0x12                         /* SYN+ACK */
    cmp al, 0x12
    jne .tc_next
    /* rcv_nxt = their seq + 1 (wire BE → host LE lo/hi words) */
    mov al, [bx + 7]
    mov ah, [bx + 6]
    mov [rcv_nxt], ax
    mov al, [bx + 5]
    mov ah, [bx + 4]
    mov [rcv_nxt + 2], ax
    add word ptr [rcv_nxt], 1
    adc word ptr [rcv_nxt + 2], 0
    mov word ptr [snd_nxt], 1
    mov word ptr [snd_nxt + 2], 0
    mov word ptr [tcp_flags], 0x10       /* ACK */
    mov word ptr [tcp_payload_len], 0
    call tcp_send_segment
    mov byte ptr [tcp_state], ST_ESTAB
    clc
    jmp .tc_done
.tc_to:
.tc_next:
    dec word ptr [pkt_tries]
    jnz .tc_wait
.tc_fail:
    mov byte ptr [tcp_state], ST_CLOSED
    stc
.tc_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* CF clear if rx_buf is IPv4/TCP for our 4-tuple; sets tcp_off → TCP hdr offset in rx_buf. */
tcp_is_ours:
    push ax
    push bx
    cmp word ptr [rx_buf + 12], 0x0008
    jne .tio_no
    cmp byte ptr [rx_buf + 23], 6
    jne .tio_no
    mov al, [rx_buf + 14]
    xor ah, ah
    and al, 0x0F
    shl ax, 1
    shl ax, 1
    add ax, 14
    mov [tcp_off], ax
    lea bx, [rx_buf]
    add bx, ax
    /* remote sport must match target_port; dport = local */
    mov ah, [bx]
    mov al, [bx + 1]
    cmp ax, [target_port]
    jne .tio_no
    mov ah, [bx + 2]
    mov al, [bx + 3]
    cmp ax, [local_port]
    jne .tio_no
    clc
    jmp .tio_done
.tio_no:
    stc
.tio_done:
    pop bx
    pop ax
    ret

/*
 * Send TCP segment. Uses tcp_flags, tcp_payload_len, payload at tcp_data.
 * snd_nxt is seq for this segment; advanced by payload (+1 if SYN/FIN).
 */
tcp_send_segment:
    push ax
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

    /* IPv4 */
    mov al, 0x45
    stosb
    xor al, al
    stosb
    mov ax, [tcp_payload_len]
    add ax, IP_HDR + TCP_HDR
    xchg al, ah
    stosw
    mov ax, [ip_ident]
    inc word ptr [ip_ident]
    xchg al, ah
    stosw
    xor ax, ax
    stosw
    mov al, 64
    stosb
    mov al, 6
    stosb
    xor ax, ax
    stosw
    lea si, [my_ip]
    movsw
    movsw
    lea si, [target_ip]
    movsw
    movsw

    /* TCP */
    mov ax, [local_port]
    xchg al, ah
    stosw
    mov ax, [target_port]
    xchg al, ah
    stosw
    /* seq = snd_nxt big-endian */
    mov ax, [snd_nxt + 2]
    xchg al, ah
    stosw
    mov ax, [snd_nxt]
    xchg al, ah
    stosw
    /* ack = rcv_nxt */
    mov ax, [rcv_nxt + 2]
    xchg al, ah
    stosw
    mov ax, [rcv_nxt]
    xchg al, ah
    stosw
    mov al, 0x50
    stosb
    mov al, [tcp_flags]
    stosb
    mov ax, 0xFFFF
    stosw
    xor ax, ax
    stosw                       /* checksum placeholder */
    xor ax, ax
    stosw                       /* urgent */

    mov cx, [tcp_payload_len]
    jcxz .tss_no_pay
    lea si, [tcp_data]
    rep movsb
.tss_no_pay:

    /* IP checksum */
    lea si, [tx_buf + 14]
    mov cx, 20
    call inet_csum
    mov [tx_buf + 24], ah
    mov [tx_buf + 25], al

    /* TCP checksum with pseudo-header in csum_buf */
    lea di, [csum_buf]
    lea si, [my_ip]
    movsw
    movsw
    lea si, [target_ip]
    movsw
    movsw
    xor al, al
    stosb
    mov al, 6
    stosb
    mov ax, [tcp_payload_len]
    add ax, TCP_HDR
    xchg al, ah
    stosw
    lea si, [tx_buf + 14 + IP_HDR]
    mov cx, [tcp_payload_len]
    add cx, TCP_HDR
    rep movsb
    lea si, [csum_buf]
    mov cx, [tcp_payload_len]
    add cx, TCP_HDR + 12
    call inet_csum
    mov [tx_buf + 14 + IP_HDR + 16], ah
    mov [tx_buf + 14 + IP_HDR + 17], al

    mov cx, [tcp_payload_len]
    add cx, ETH_HDR + IP_HDR + TCP_HDR
    cmp cx, 60
    jae .tss_tx
    lea di, [tx_buf]
    add di, cx
    mov ax, 60
    sub ax, cx
    mov cx, ax
    xor al, al
    rep stosb
    mov cx, 60
.tss_tx:
    lea si, [tx_buf]
    call nic_transmit

    /* advance snd_nxt */
    mov ax, [tcp_payload_len]
    add [snd_nxt], ax
    adc word ptr [snd_nxt + 2], 0
    test byte ptr [tcp_flags], 0x03      /* FIN|SYN */
    jz .tss_done
    add word ptr [snd_nxt], 1
    adc word ptr [snd_nxt + 2], 0
.tss_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* Interactive session until FIN/RST/abort. */
session_loop:
    push ax
    push bx
    push cx
    push dx
    push si
.sl_loop:
    call check_abort
    jnz .sl_done
    call nic_rx
    jc .sl_keys
.sl_got:
    call handle_tcp_rx
    cmp byte ptr [tcp_state], ST_ESTAB
    jb .sl_done
    /* Drain any already-queued frames before key poll / long wait. */
    mov word ptr [drain_left], 32
.sl_drain:
    call check_abort
    jnz .sl_done
    call nic_rx_quick
    jc .sl_keys
    call handle_tcp_rx
    cmp byte ptr [tcp_state], ST_ESTAB
    jb .sl_done
    dec word ptr [drain_left]
    jnz .sl_drain
    jmp .sl_loop
.sl_keys:
    call poll_key_send
    jmp .sl_loop
.sl_done:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

handle_tcp_rx:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    call tcp_is_ours
    jc .htr_done
    lea bx, [rx_buf]
    add bx, [tcp_off]
    mov al, [bx + 13]
    test al, 0x04
    jz .htr_no_rst
    mov byte ptr [tcp_state], ST_CLOSED
    jmp .htr_done
.htr_no_rst:
    /* TCP header length (data offset); reject < 5 (20 bytes). */
    mov al, [bx + 12]
    xor ah, ah
    mov cl, 4
    shr al, cl
    cmp al, 5
    jb .htr_nodata
    shl ax, 1
    shl ax, 1
    mov cx, ax                       /* TCP hdr len */
    /* payload from IP total length (ignore eth padding) */
    mov ah, [rx_buf + 16]
    mov al, [rx_buf + 17]
    /* Clamp IP total length to received frame size. */
    cmp ax, [rx_len]
    jbe .htr_ipl_ok
    mov ax, [rx_len]
.htr_ipl_ok:
    mov dx, [tcp_off]
    sub dx, 14                       /* IP header length */
    sub ax, dx
    jbe .htr_nodata
    sub ax, cx
    jbe .htr_nodata
    mov dx, [tcp_off]
    add dx, cx                       /* payload offset in frame */
    cmp dx, [rx_len]
    jae .htr_nodata
    mov bx, [rx_len]
    sub bx, dx                       /* max bytes actually in rx_buf */
    cmp ax, bx
    jbe .htr_plen_ok
    mov ax, bx
.htr_plen_ok:
    test ax, ax
    jz .htr_nodata
    lea si, [rx_buf]
    add si, dx
    mov cx, ax                       /* payload len */
    call nvt_print
    add [rcv_nxt], ax
    adc word ptr [rcv_nxt + 2], 0
    mov word ptr [tcp_flags], 0x10
    mov word ptr [tcp_payload_len], 0
    call tcp_send_segment
.htr_nodata:
    lea bx, [rx_buf]
    add bx, [tcp_off]
    test byte ptr [bx + 13], 0x01
    jz .htr_done
    /* FIN: bump rcv, ACK, done */
    add word ptr [rcv_nxt], 1
    adc word ptr [rcv_nxt + 2], 0
    mov word ptr [tcp_flags], 0x10
    mov word ptr [tcp_payload_len], 0
    call tcp_send_segment
    mov byte ptr [tcp_state], ST_CLOSED
.htr_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* CX=len, SI=data — print; skip IAC; interpret ANSI CSI (ESC[...). */
nvt_print:
    push ax
    push bx
    push cx
    push dx
    push si
.nv_loop:
    test cx, cx
    jnz .nv_have
    jmp .nv_done
.nv_have:
    /* Resume in-progress IAC before ANSI / payload bytes. */
    mov bl, [iac_state]
    test bl, bl
    jz .nv_noiac
    jmp .nv_iac_resume
.nv_noiac:
    lodsb
    dec cx
    mov bl, [ansi_state]
    test bl, bl
    jnz .nv_escish
    cmp al, 0xFF
    je .nv_iac
    cmp al, 0x1B
    jne .nv_putc
    mov byte ptr [ansi_state], 1
    jmp .nv_loop
.nv_putc:
    /* ASCII NVT: only print printable 7-bit + CR/LF/BS/TAB. */
    cmp al, 0x7F
    jae .nv_loop
    cmp al, 0x20
    jae .nv_putc_ok
    cmp al, 13
    je .nv_putc_ok
    cmp al, 10
    je .nv_putc_ok
    cmp al, 8
    je .nv_putc_ok
    cmp al, 9
    je .nv_putc_ok
    jmp .nv_loop
.nv_putc_ok:
    call con_putc
    jmp .nv_loop

.nv_escish:
    cmp bl, 1
    jne .nv_csi
    cmp al, '['
    jne .nv_esc_bad
    mov byte ptr [ansi_state], 2
    mov word ptr [ansi_p1], 0
    mov word ptr [ansi_p2], 0
    mov byte ptr [ansi_which], 0
    jmp .nv_loop
.nv_esc_bad:
    mov byte ptr [ansi_state], 0
    cmp al, '7'
    je .nv_sc_save
    cmp al, '8'
    je .nv_sc_rest
    inc cx
    dec si
    jmp .nv_loop
.nv_sc_save:
    mov al, [cur_row]
    mov [save_row], al
    mov al, [cur_col]
    mov [save_col], al
    jmp .nv_loop
.nv_sc_rest:
    mov al, [save_row]
    mov [cur_row], al
    mov al, [save_col]
    mov [cur_col], al
    jmp .nv_loop

.nv_csi:
    /* Nested ESC restarts the sequence (do not eat it as a CSI byte). */
    cmp al, 0x1B
    jne .nv_csi_body
    mov byte ptr [ansi_state], 1
    jmp .nv_loop
.nv_csi_body:
    cmp al, '0'
    jb .nv_csi_other
    cmp al, '9'
    ja .nv_csi_sep
    sub al, '0'
    mov ah, 0
    mov bx, ax
    cmp byte ptr [ansi_which], 0
    jne .nv_d2
    mov ax, [ansi_p1]
    mov dx, 10
    mul dx
    add ax, bx
    mov [ansi_p1], ax
    jmp .nv_loop
.nv_d2:
    mov ax, [ansi_p2]
    mov dx, 10
    mul dx
    add ax, bx
    mov [ansi_p2], ax
    jmp .nv_loop
.nv_csi_sep:
    cmp al, ';'
    jne .nv_csi_other
    mov byte ptr [ansi_which], 1
    jmp .nv_loop
.nv_csi_other:
    cmp al, 0x40
    jb .nv_loop
    cmp al, 0x7E
    ja .nv_loop
    mov byte ptr [ansi_state], 0
    call ansi_csi_cmd
    jmp .nv_loop

/* ---- Telnet IAC (stateful across TCP segments) ----
 * iac_state: 0=none, 1=got FF, 2=need option (WILL/WONT/DO/DONT),
 *            3=subnegotiation until IAC SE
 */
.nv_iac:
    mov byte ptr [iac_state], 1
.nv_iac_resume:
    mov bl, [iac_state]
    cmp bl, 1
    je .nv_iac_cmd
    cmp bl, 2
    je .nv_iac_opt
    /* state 3: SB body */
    jmp .nv_iac_sb

.nv_iac_cmd:
    test cx, cx
    jnz .nv_iac_cmd1
    jmp .nv_done
.nv_iac_cmd1:
    lodsb
    dec cx
    cmp al, 0xFF
    jne .nv_iac_cmd2
    mov byte ptr [iac_state], 0
    mov al, 0xFF
    call con_putc
    jmp .nv_loop
.nv_iac_cmd2:
    cmp al, 250                       /* SB */
    jne .nv_iac_cmd3
    mov byte ptr [iac_state], 3
    jmp .nv_iac_resume
.nv_iac_cmd3:
    cmp al, 251                       /* WILL */
    jb .nv_iac_done_cmd
    cmp al, 254                       /* DONT */
    ja .nv_iac_done_cmd
    mov [iac_cmd], al
    mov byte ptr [iac_state], 2
    jmp .nv_iac_resume
.nv_iac_done_cmd:
    mov byte ptr [iac_state], 0
    jmp .nv_loop

.nv_iac_opt:
    test cx, cx
    jnz .nv_iac_opt1
    jmp .nv_done
.nv_iac_opt1:
    lodsb
    dec cx
    mov byte ptr [iac_state], 0
    /* Swallow WILL/WONT/DO/DONT + option; do not reply inline (avoids
     * re-entering the TX path while SI still walks rx_buf). */
    jmp .nv_loop

.nv_iac_sb:
    test cx, cx
    jnz .nv_iac_sb1
    jmp .nv_done
.nv_iac_sb1:
    lodsb
    dec cx
    cmp al, 0xFF
    jne .nv_loop                      /* stay in SB */
    /* got IAC inside SB — need SE (240) */
    test cx, cx
    jnz .nv_iac_sb2
    mov byte ptr [iac_state], 1       /* treat next as cmd */
    jmp .nv_done
.nv_iac_sb2:
    lodsb
    dec cx
    cmp al, 240                       /* SE */
    jne .nv_iac_sb_nest
    mov byte ptr [iac_state], 0
    jmp .nv_loop
.nv_iac_sb_nest:
    cmp al, 0xFF
    je .nv_loop                       /* IAC IAC inside SB */
    /* IAC + other command aborted SB */
    mov byte ptr [iac_state], 0
    inc cx
    dec si
    mov byte ptr [iac_state], 1
    jmp .nv_iac_resume

.nv_done:
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* AL = CSI final byte; uses ansi_p1/ansi_p2. */
ansi_csi_cmd:
    push ax
    push bx
    push cx
    push dx
    cmp al, 'H'
    je .ac_cup
    cmp al, 'f'
    je .ac_cup
    cmp al, 'J'
    je .ac_ed
    cmp al, 'K'
    je .ac_el
    cmp al, 'A'
    je .ac_cuu
    cmp al, 'B'
    je .ac_cud
    cmp al, 'C'
    je .ac_cuf
    cmp al, 'D'
    je .ac_cub
    cmp al, 'G'
    je .ac_cha
    cmp al, 'm'
    je .ac_done                    /* SGR: ignore */
    cmp al, 'h'
    je .ac_done                    /* SM / DECSET */
    cmp al, 'l'
    je .ac_done                    /* RM / DECRST */
    jmp .ac_done
.ac_cup:
    mov ax, [ansi_p1]
    test ax, ax
    jnz .ac_cup_r
    mov ax, 1
.ac_cup_r:
    dec ax
    cmp ax, 24
    jbe .ac_cup_r2
    mov ax, 24
.ac_cup_r2:
    mov [cur_row], al
    mov ax, [ansi_p2]
    test ax, ax
    jnz .ac_cup_c
    mov ax, 1
.ac_cup_c:
    dec ax
    cmp ax, 79
    jbe .ac_cup_c2
    mov ax, 79
.ac_cup_c2:
    mov [cur_col], al
    /* Home only — do NOT clear here. Asciimation sends ESC[H every
     * frame; full clears flicker and stall RX into garbage. */
    jmp .ac_done
.ac_cha:
    /* ESC[nG — cursor horizontal absolute (col, 1-based) */
    mov ax, [ansi_p1]
    test ax, ax
    jnz .ac_cha_n
    mov ax, 1
.ac_cha_n:
    dec ax
    cmp ax, 79
    jbe .ac_cha_ok
    mov ax, 79
.ac_cha_ok:
    mov [cur_col], al
    jmp .ac_done
.ac_ed:
    call ansi_get_cursor
    mov ax, [ansi_p1]
    cmp ax, 2
    je .ac_ed_all
    cmp ax, 1
    je .ac_ed_bos
    /* 0 / omitted from home == full clear (Star Wars: ESC[H ESC[J]) */
    cmp byte ptr [cur_row], 0
    jne .ac_ed_eos
    cmp byte ptr [cur_col], 0
    jne .ac_ed_eos
.ac_ed_all:
    call con_clear_all
    jmp .ac_done
.ac_ed_eos:
    mov ch, dh
    mov cl, dl
    mov dh, 24
    mov dl, 79
    jmp .ac_ed_bios
.ac_ed_bos:
    mov ch, 0
    mov cl, 0
.ac_ed_bios:
    mov bh, 0x07
    mov ax, 0x0600
    int 0x10
    jmp .ac_done
.ac_el:
    call ansi_get_cursor
    mov ax, [ansi_p1]
    cmp ax, 2
    je .ac_el_all
    cmp ax, 1
    je .ac_el_bol
    mov ch, dh
    mov cl, dl
    mov dl, 79
    jmp .ac_el_do
.ac_el_bol:
    mov ch, dh
    xor cl, cl
    jmp .ac_el_do
.ac_el_all:
    mov ch, dh
    xor cl, cl
    mov dl, 79
.ac_el_do:
    /* Erase via B800 for consistency with putc. */
    push si
    push di
    push es
    mov al, dh
    xor ah, ah
    mov bl, 80
    mul bl
    mov bl, cl
    xor bh, bh
    add ax, bx
    shl ax, 1
    mov di, ax
    mov al, dl
    xor ah, ah
    sub al, cl
    inc ax
    mov cx, ax
    mov ax, 0xB800
    mov es, ax
    mov ax, 0x0720
    cld
    rep stosw
    pop es
    pop di
    pop si
    jmp .ac_done
.ac_cuu:
    call ansi_get_cursor
    mov ax, [ansi_p1]
    test ax, ax
    jnz .ac_cuu_n
    mov ax, 1
.ac_cuu_n:
    sub dh, al
    jnc .ac_set
    xor dh, dh
    jmp .ac_set
.ac_cud:
    call ansi_get_cursor
    mov ax, [ansi_p1]
    test ax, ax
    jnz .ac_cud_n
    mov ax, 1
.ac_cud_n:
    add dh, al
    cmp dh, 24
    jbe .ac_set
    mov dh, 24
    jmp .ac_set
.ac_cuf:
    call ansi_get_cursor
    mov ax, [ansi_p1]
    test ax, ax
    jnz .ac_cuf_n
    mov ax, 1
.ac_cuf_n:
    add dl, al
    cmp dl, 79
    jbe .ac_set
    mov dl, 79
    jmp .ac_set
.ac_cub:
    call ansi_get_cursor
    mov ax, [ansi_p1]
    test ax, ax
    jnz .ac_cub_n
    mov ax, 1
.ac_cub_n:
    sub dl, al
    jnc .ac_set
    xor dl, dl
.ac_set:
    mov [cur_row], dh
    mov [cur_col], dl
.ac_done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* DH/DL = software cursor (do NOT query BIOS — B800 path leaves it stale). */
ansi_get_cursor:
    mov dh, [cur_row]
    mov dl, [cur_col]
    ret

con_sync_cursor:
    push ax
    push bx
    push dx
    mov dh, [cur_row]
    mov dl, [cur_col]
    xor bh, bh
    mov ah, 0x02
    int 0x10
    pop dx
    pop bx
    pop ax
    ret

/* Fill B800 with spaces + attr 07; home software cursor (no BIOS). */
con_clear_all:
    push ax
    push cx
    push di
    push es
    mov ax, 0xB800
    mov es, ax
    xor di, di
    mov ax, 0x0720
    mov cx, (80 * 25)
    cld
    rep stosw
    mov byte ptr [cur_row], 0
    mov byte ptr [cur_col], 0
    pop es
    pop di
    pop cx
    pop ax
    ret

/* Erase from cur_col through column 79 on cur_row. */
con_erase_eol:
    push ax
    push bx
    push cx
    push di
    push es
    mov al, [cur_col]
    cmp al, 80
    jae .ce_done
    mov bl, al
    mov al, [cur_row]
    xor ah, ah
    mov cl, 80
    mul cl
    mov bh, 0
    add ax, bx
    shl ax, 1
    mov di, ax
    mov cx, 80
    sub cl, bl
    xor ch, ch
    mov ax, 0xB800
    mov es, ax
    mov ax, 0x0720
    cld
    rep stosw
.ce_done:
    pop es
    pop di
    pop cx
    pop bx
    pop ax
    ret

/*
 * Fast console putchar via CGA text buffer (B800). AL=char.
 * Handles CR/LF/BS; LF also resets column (Unix newline).
 */
con_putc:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov byte ptr [con_ch], al
    cmp al, 13
    je .cp_cr
    cmp al, 10
    je .cp_lf
    cmp al, 8
    je .cp_bs
    cmp al, 9
    je .cp_tab
    /* offset = (row*80+col)*2 */
    mov al, [cur_row]
    xor ah, ah
    mov cl, 80
    mul cl
    mov bl, [cur_col]
    xor bh, bh
    add ax, bx
    shl ax, 1
    mov bx, ax
    mov ax, 0xB800
    mov es, ax
    mov al, [con_ch]
    mov ah, 0x07
    mov es:[bx], ax
    inc byte ptr [cur_col]
    cmp byte ptr [cur_col], 80
    jb .cp_done
    mov byte ptr [cur_col], 0
    jmp .cp_lf_row
.cp_tab:
    mov al, [cur_col]
    add al, 8
    and al, 0xF8
    cmp al, 80
    jb .cp_tab_ok
    mov byte ptr [cur_col], 0
    jmp .cp_lf_row
.cp_tab_ok:
    mov [cur_col], al
    jmp .cp_done
.cp_cr:
    /* Wipe to end of line so 73-col asciimation cannot leave a dirty margin. */
    call con_erase_eol
    mov byte ptr [cur_col], 0
    jmp .cp_done
.cp_bs:
    cmp byte ptr [cur_col], 0
    je .cp_done
    dec byte ptr [cur_col]
    jmp .cp_done
.cp_lf:
    mov byte ptr [cur_col], 0          /* newline = CR+LF */
.cp_lf_row:
    inc byte ptr [cur_row]
    cmp byte ptr [cur_row], 25
    jb .cp_done
    mov byte ptr [cur_row], 24
    /* scroll up one line via B800 — must not clobber caller's SI (nvt_print). */
    push ds
    cld
    mov ax, 0xB800
    mov ds, ax
    mov es, ax
    xor di, di
    mov si, 160
    mov cx, (24 * 80)
    rep movsw
    mov ax, 0x0720
    mov cx, 80
    rep stosw
    pop ds
.cp_done:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

poll_key_send:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push cs
    pop ds
    mov ah, 0x01
    int 0x16
    jz .pks_done
    mov ah, 0x00
    int 0x16
    cmp al, 0
    je .pks_done
    cmp al, 3                        /* Ctrl-C */
    jne .pks_ch
    mov byte ptr [abort_flag], 1
    jmp .pks_done
.pks_ch:
    /* echo locally */
    mov dl, al
    mov ah, 0x02
    int 0x21
    push cs
    pop ds
    cmp al, 13
    jne .pks_one
    mov dl, 10
    mov ah, 0x02
    int 0x21
    push cs
    pop ds
    mov byte ptr [tcp_data], 13
    mov byte ptr [tcp_data + 1], 10
    mov word ptr [tcp_payload_len], 2
    jmp .pks_tx
.pks_one:
    mov byte ptr [tcp_data], al
    mov word ptr [tcp_payload_len], 1
.pks_tx:
    mov word ptr [tcp_flags], 0x18   /* PSH+ACK */
    call tcp_send_segment
.pks_done:
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* Parse: TELNET host [port] — host is IPv4 or DNS hostname. */
parse_args:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov word ptr [target_port], DEF_PORT
    mov byte ptr [target_set], 0
    mov byte ptr [hostname_set], 0
    mov byte ptr [port_set], 0
    mov byte ptr [args_err], 0
    mov byte ptr [target_ip], 0
    mov byte ptr [target_ip + 1], 0
    mov byte ptr [target_ip + 2], 0
    mov byte ptr [target_ip + 3], 0
    mov byte ptr [hostname], 0

    mov si, 0x80
    xor cx, cx
    mov cl, [si]
    test cl, cl
    jz .pa_done
    inc si

.pa_tok:
    call skip_spaces
    test cx, cx
    jz .pa_done
    cmp byte ptr [target_set], 0
    jne .pa_maybe_port
    cmp byte ptr [hostname_set], 0
    jne .pa_maybe_port

    push si
    push cx
    lea di, [target_ip]
    mov byte ptr [di], 0
    mov byte ptr [di + 1], 0
    mov byte ptr [di + 2], 0
    mov byte ptr [di + 3], 0
    mov bl, 4
.pa_oct:
    test cx, cx
    jz .pa_try_host
    mov dl, [si]
    cmp dl, '0'
    jb .pa_try_host
    cmp dl, '9'
    ja .pa_try_host
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
    ja .pa_try_host
    mov [di], al
    jmp .pa_dig
.pa_term:
    inc di
    dec bl
    jz .pa_ip_done
    test cx, cx
    jz .pa_try_host
    lodsb
    dec cx
    cmp al, '.'
    je .pa_oct
.pa_try_host:
    pop cx
    pop si
    call copy_hostname
    jc .pa_bad
    mov byte ptr [hostname_set], 1
    jmp .pa_tok
.pa_ip_done:
    add sp, 4                        /* drop saved SI/CX */
    mov byte ptr [target_set], 1
    jmp .pa_tok

.pa_maybe_port:
    cmp byte ptr [port_set], 0
    jne .pa_bad
    mov bl, [si]
    cmp bl, '0'
    jb .pa_bad
    cmp bl, '9'
    ja .pa_bad
    call parse_num
    test ax, ax
    jz .pa_bad
    mov [target_port], ax
    mov byte ptr [port_set], 1
    jmp .pa_tok

.pa_bad:
    mov byte ptr [args_err], 1
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
    mov dh, 0
    push dx
    mov bx, 10
    mul bx
    pop dx
    add ax, dx
    jmp .pn
.pn_d:
    ret

.include "firmware/src/dos/inc/netlease.inc"
.include "firmware/src/dos/inc/nettsr.inc"
.include "firmware/src/dos/inc/netutil.inc"
.include "firmware/src/dos/inc/ne2000.inc"
.include "firmware/src/dos/inc/dns.inc"

msg_help:
    .ascii "Usage: TELNET host [port]\r\n"
    .ascii "Requires DHCP lease (LEASE.DAT). Default port 23.\r\n"
    .ascii "host may be an IPv4 address or DNS hostname.\r\n$"
msg_resolving:
    .ascii "Resolving hostname...\r\n$"
msg_dns_fail:
    .ascii "TELNET: could not resolve host.\r\n$"
msg_connected:
    .ascii "Connected.\r\n$"
msg_no_lease:
    .ascii "TELNET: no DHCP lease. Run DHCP first.\r\n$"
msg_nic:
    .ascii "TELNET: NIC init failed.\r\n$"
msg_arp:
    .ascii "TELNET: ARP failed.\r\n$"
msg_conn:
    .ascii "TELNET: connect failed.\r\n$"
msg_break:
    .ascii "\r\n^C\r\n$"

lease_path:
    .asciz "LEASE.DAT"
lease_handle:
    .word 0
lease_buf:
    .space LEASE_SIZE, 0
net_use_tsr:
    .byte 0

my_mac:
    .space 6, 0
gw_mac:
    .space 6, 0
my_ip:
    .byte 0, 0, 0, 0
gateway_ip:
    .byte 0, 0, 0, 0
dns_ip:
    .byte 0, 0, 0, 0
target_ip:
    .byte 0, 0, 0, 0
target_port:
    .word DEF_PORT
local_port:
    .word 0
target_set:
    .byte 0
hostname_set:
    .byte 0
port_set:
    .byte 0
args_err:
    .byte 0
hostname:
    .space DNS_HOST_MAX, 0
dns_id:
    .word 0
dns_tries:
    .word 0
abort_flag:
    .byte 0
tcp_state:
    .byte 0
poll_left:
    .word 0
pkt_tries:
    .word 0
ip_ident:
    .word 1
tx_len:
    .word 0
rx_len:
    .word 0
tcp_off:
    .word 0
tcp_flags:
    .word 0
tcp_payload_len:
    .word 0
ansi_state:
    .byte 0
ansi_which:
    .byte 0
ansi_p1:
    .word 0
ansi_p2:
    .word 0
iac_state:
    .byte 0
iac_cmd:
    .byte 0
cur_row:
    .byte 0
cur_col:
    .byte 0
save_row:
    .byte 0
save_col:
    .byte 0
con_ch:
    .byte 0
drain_left:
    .word 0
snd_nxt:
    .word 0, 0
rcv_nxt:
    .word 0, 0
tcp_data:
    .space 256, 0
csum_buf:
    .space 1600, 0
tx_buf:
    .space 1600, 0
rx_buf:
    .space 1600, 0
