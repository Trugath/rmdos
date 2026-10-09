/*
 * DNS A-record resolver for rmDOS PING/TELNET.
 *
 * copy_hostname: DS:SI length CX — copy next token into hostname ASCIZ; CF=bad.
 * dns_resolve: hostname[] ASCIZ → target_ip[4]; CF=fail.
 * Requires near: dns_ip, my_ip, my_mac, gw_mac, target_ip, hostname,
 * dns_id, dns_tries, tx_buf, rx_buf, tx_len, rx_len, poll_left, check_abort;
 * and nic_transmit / nic_rx / inet_csum / get_ticks.
 */

.ifndef DNS_HOST_MAX
.set DNS_HOST_MAX, 96
.endif

/* Copy hostname token at SI (len CX) into hostname[]. Updates SI/CX. */
copy_hostname:
    push ax
    push bx
    push dx
    push di
    lea di, [hostname]
    xor bx, bx
.ch_loop:
    test cx, cx
    jz .ch_end
    mov al, [si]
    cmp al, ' '
    je .ch_end
    cmp al, 9
    je .ch_end
    call host_char_ok
    jc .ch_bad
    cmp bx, DNS_HOST_MAX - 1
    jae .ch_bad
    mov [di], al
    inc di
    inc bx
    inc si
    dec cx
    jmp .ch_loop
.ch_end:
    test bx, bx
    jz .ch_bad
    mov byte ptr [di], 0
    clc
    jmp .ch_done
.ch_bad:
    mov byte ptr [hostname], 0
    stc
.ch_done:
    pop di
    pop dx
    pop bx
    pop ax
    ret

/* AL = char; CF clear if allowed in hostname. */
host_char_ok:
    cmp al, '.'
    je .hc_ok
    cmp al, '-'
    je .hc_ok
    cmp al, '0'
    jb .hc_bad
    cmp al, '9'
    jbe .hc_ok
    or al, 0x20
    cmp al, 'a'
    jb .hc_bad
    cmp al, 'z'
    jbe .hc_ok
.hc_bad:
    stc
    ret
.hc_ok:
    clc
    ret

dns_resolve:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    /* reject empty / zero DNS server */
    mov ax, [dns_ip]
    or ax, [dns_ip + 2]
    jnz .dr_have_dns
    jmp .dr_fail
.dr_have_dns:
    mov al, [si]
    test al, al
    jnz .dr_build
    jmp .dr_fail

.dr_build:
    call get_ticks
    mov [dns_id], ax
    call dns_send_query
    jc .dr_fail

    mov word ptr [dns_tries], 12
.dr_wait:
    call check_abort
    jnz .dr_fail
    call nic_rx
    jc .dr_to
    call dns_parse_reply
    jnc .dr_ok
    dec word ptr [dns_tries]
    jnz .dr_wait
    jmp .dr_fail
.dr_to:
    dec word ptr [dns_tries]
    jz .dr_fail
    /* retransmit every other timeout */
    test word ptr [dns_tries], 1
    jnz .dr_wait
    call dns_send_query
    jmp .dr_wait

.dr_ok:
    clc
    jmp .dr_done
.dr_fail:
    stc
.dr_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* SI = hostname ASCIZ. Build UDP DNS query into tx_buf and transmit. */
dns_send_query:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    lea di, [tx_buf]
    /* eth: dst gw, src me, IPv4 */
    lea si, [gw_mac]
    mov cx, 3
    rep movsw
    lea si, [my_mac]
    mov cx, 3
    rep movsw
    mov ax, 0x0008
    stosw

    /* remember IP header start */
    mov bx, di

    mov al, 0x45
    stosb
    xor al, al
    stosb
    /* total length placeholder */
    xor ax, ax
    stosw
    mov ax, [dns_id]
    xchg al, ah
    stosw
    xor ax, ax
    stosw                       /* flags */
    mov al, 64
    stosb
    mov al, 17                   /* UDP */
    stosb
    xor ax, ax
    stosw                       /* checksum placeholder */
    lea si, [my_ip]
    movsw
    movsw
    lea si, [dns_ip]
    movsw
    movsw

    /* UDP header */
    mov ax, [dns_id]
    or ax, 0xC000                /* ephemeral sport */
    xchg al, ah
    stosw
    mov ax, 53
    xchg al, ah
    stosw
    xor ax, ax
    stosw                       /* UDP length placeholder */
    xor ax, ax
    stosw                       /* UDP checksum = 0 */

    /* DNS header */
    mov ax, [dns_id]
    xchg al, ah
    stosw
    mov ax, 0x0001               /* RD */
    xchg al, ah
    stosw
    mov ax, 0x0100               /* QDCOUNT=1 */
    stosw
    xor ax, ax
    stosw
    stosw
    stosw

    /* QNAME from hostname at [bp saved] — recover original SI from stack.
     * We pushed si after dx; stack top after our pushes: ... original SI at [sp+?]
     * Simpler: hostname is always in near `hostname` buffer for callers. */
    lea si, [hostname]
.dr_lab:
    mov dx, si
.dr_lab_scan:
    mov al, [si]
    test al, al
    jz .dr_lab_end
    cmp al, '.'
    je .dr_lab_emit
    inc si
    jmp .dr_lab_scan
.dr_lab_emit:
    mov ax, si
    sub ax, dx
    cmp ax, 63
    ja .dr_tx_fail
    test ax, ax
    jz .dr_tx_fail
    stosb                       /* label length (AL) */
    mov cx, ax
    push si
    mov si, dx
    rep movsb
    pop si
    inc si                       /* skip dot */
    jmp .dr_lab
.dr_lab_end:
    mov ax, si
    sub ax, dx
    jz .dr_root
    cmp ax, 63
    ja .dr_tx_fail
    stosb
    mov cx, ax
    mov si, dx
    rep movsb
.dr_root:
    xor al, al
    stosb
    mov ax, 0x0100               /* QTYPE A */
    stosw
    mov ax, 0x0100               /* QCLASS IN */
    stosw

    /* lengths: DI = end, BX = IP start */
    mov ax, di
    sub ax, bx                   /* IP total length */
    mov [bx + 2], ah
    mov [bx + 3], al
    /* UDP length = IP total - 20 */
    sub ax, 20
    mov [bx + 24], ah
    mov [bx + 25], al

    /* IP checksum */
    push di
    lea si, [tx_buf + 14]
    mov cx, 20
    call inet_csum
    mov [tx_buf + 24], ah
    mov [tx_buf + 25], al
    pop di

    mov cx, di
    lea si, [tx_buf]
    sub cx, si
    cmp cx, 60
    jae .dr_tx
    mov cx, 60
.dr_tx:
    call nic_transmit
    clc
    jmp .dr_tx_done
.dr_tx_fail:
    stc
.dr_tx_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* Parse rx_buf as DNS response for dns_id; copy first A to target_ip. CF=fail. */
dns_parse_reply:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    cmp word ptr [rx_buf + 12], 0x0008
    jne .dp_fail
    cmp byte ptr [rx_buf + 23], 17
    jne .dp_fail
    /* UDP dport must be our ephemeral */
    mov ax, [dns_id]
    or ax, 0xC000
    mov dh, [rx_buf + 36]
    mov dl, [rx_buf + 37]
    cmp dx, ax
    jne .dp_fail
    mov dh, [rx_buf + 34]
    mov dl, [rx_buf + 35]
    cmp dx, 53
    jne .dp_fail

    lea si, [rx_buf + 42]        /* DNS payload after eth+IP+UDP */
    mov ah, [si]
    mov al, [si + 1]
    cmp ax, [dns_id]
    jne .dp_fail
    /* must be response */
    test byte ptr [si + 2], 0x80
    jz .dp_fail
    /* RCODE */
    mov al, [si + 3]
    and al, 0x0F
    jnz .dp_fail
    mov ah, [si + 6]
    mov al, [si + 7]             /* ANCOUNT */
    test ax, ax
    jz .dp_fail

    /* skip question: start at SI+12 */
    add si, 12
.dp_skip_q:
    lodsb
    test al, al
    jz .dp_q_done
    test al, 0xC0
    jnz .dp_q_ptr
    mov ah, 0
    add si, ax
    jmp .dp_skip_q
.dp_q_ptr:
    inc si                       /* second pointer byte */
.dp_q_done:
    add si, 4                    /* QTYPE + QCLASS */

    /* walk answers for A/IN */
    mov ch, [rx_buf + 42 + 6]
    mov cl, [rx_buf + 42 + 7]    /* ANCOUNT */
.dp_ans:
    jcxz .dp_fail
    /* name */
    mov al, [si]
    test al, 0xC0
    jz .dp_name_labels
    add si, 2
    jmp .dp_rr
.dp_name_labels:
    lodsb
    test al, al
    jz .dp_rr
    mov ah, 0
    add si, ax
    jmp .dp_name_labels
.dp_rr:
    mov dh, [si]
    mov dl, [si + 1]             /* TYPE */
    mov ah, [si + 2]
    mov al, [si + 3]             /* CLASS */
    add si, 8                    /* type+class+ttl */
    mov bh, [si]
    mov bl, [si + 1]             /* RDLENGTH */
    add si, 2
    cmp dx, 1
    jne .dp_skip_rdata
    cmp ax, 1
    jne .dp_skip_rdata
    cmp bx, 4
    jne .dp_skip_rdata
    lea di, [target_ip]
    movsw
    movsw
    clc
    jmp .dp_done
.dp_skip_rdata:
    add si, bx
    dec cx
    jmp .dp_ans

.dp_fail:
    stc
.dp_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret
