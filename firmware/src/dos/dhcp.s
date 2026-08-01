.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * DHCP.COM — DHCP client via DE-220 NE2000 @ 0x300.
 * Discover → Offer → Request → Ack against the k8086 virtual gateway.
 * Writes LEASE.DAT for PING and other tools.
 * Usage: DHCP
 */

.set BOOT_LEN, 240
.set OPT_MAX, 64
.set DHCP_LEN, (BOOT_LEN + OPT_MAX)
.set UDP_LEN, (8 + DHCP_LEN)
.set IP_TOTAL, (20 + UDP_LEN)
.set ETH_TOTAL, (14 + IP_TOTAL)
.set LEASE_SIZE, 24

.include "firmware/src/dos/inc/netlease_defs.inc"

_start:
    push cs
    pop ds
    push cs
    pop es

    /* no arguments expected */
    mov si, 0x80
    mov cl, [si]
    test cl, cl
    jz .go
    /* allow trailing spaces only */
    inc si
.chk_sp:
    test cl, cl
    jz .go
    mov al, [si]
    cmp al, ' '
    je .sp_ok
    cmp al, 9
    je .sp_ok
    jmp .show_help
.sp_ok:
    inc si
    dec cl
    jmp .chk_sp

.show_help:
    mov ah, 0x09
    lea dx, [msg_help]
    int 0x21
    mov ax, 0x4C01
    int 0x21

.go:
    call net_probe
    call install_break
    call nic_init
    jc .hard_fail

    mov ah, 0x09
    lea dx, [msg_start]
    int 0x21

    call do_dhcp
    jc .fail

    call print_lease
    call save_lease
    jc .lease_write_fail
    mov ax, 0x4C00
    int 0x21

.lease_write_fail:
    mov ah, 0x09
    lea dx, [msg_lease_write]
    int 0x21
    jmp .exit_fail

.fail:
    cmp byte ptr [abort_flag], 0
    je .fail_msg
    mov ah, 0x09
    lea dx, [msg_break]
    int 0x21
    jmp .exit_fail
.fail_msg:
    mov ah, 0x09
    lea dx, [msg_fail]
    int 0x21
.exit_fail:
    mov ax, 0x4C01
    int 0x21

.hard_fail:
    mov ah, 0x09
    lea dx, [msg_nic]
    int 0x21
    jmp .exit_fail

/* ---- DHCP state machine ---- */
do_dhcp:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    call get_ticks
    mov [xid], ax
    mov [xid + 2], ax
    xor ax, 0xA5A5
    mov [xid + 2], ax

    /* Discover */
    mov ah, 0x09
    lea dx, [msg_discover]
    int 0x21
    mov byte ptr [want_type], 2
    mov byte ptr [phase], 1
    call send_discover
    mov word ptr [pkt_tries], 8
.dh_offer_wait:
    call check_abort
    jnz .dh_fail
    call nic_rx
    jc .dh_offer_to
    call parse_dhcp_reply
    jc .dh_offer_next
    /* got Offer */
    jmp .dh_request
.dh_offer_next:
    dec word ptr [pkt_tries]
    jnz .dh_offer_wait
    jmp .dh_fail
.dh_offer_to:
    dec word ptr [pkt_tries]
    jnz .dh_offer_retry
    jmp .dh_fail
.dh_offer_retry:
    call send_discover
    jmp .dh_offer_wait

.dh_request:
    mov ah, 0x09
    lea dx, [msg_request]
    int 0x21
    mov byte ptr [want_type], 5
    mov byte ptr [phase], 2
    call send_request
    mov word ptr [pkt_tries], 8
.dh_ack_wait:
    call check_abort
    jnz .dh_fail
    call nic_rx
    jc .dh_ack_to
    call parse_dhcp_reply
    jc .dh_ack_next
    clc
    jmp .dh_done
.dh_ack_next:
    dec word ptr [pkt_tries]
    jnz .dh_ack_wait
    jmp .dh_fail
.dh_ack_to:
    dec word ptr [pkt_tries]
    jnz .dh_ack_retry
    jmp .dh_fail
.dh_ack_retry:
    call send_request
    jmp .dh_ack_wait

.dh_fail:
    stc
.dh_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* ---- Discover / Request builders ---- */
send_discover:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    lea di, [dhcp_opts]
    mov al, 53
    stosb
    mov al, 1
    stosb
    mov al, 1                   /* Discover */
    stosb
    mov al, 55
    stosb
    mov al, 4
    stosb
    mov al, 1                   /* subnet */
    stosb
    mov al, 3                   /* router */
    stosb
    mov al, 6                   /* dns */
    stosb
    mov al, 51                  /* lease */
    stosb
    mov al, 0xFF
    stosb
    mov ax, di
    lea bx, [dhcp_opts]
    sub ax, bx
    mov [dhcp_opt_len], ax
    call assemble_and_tx

    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

send_request:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    lea di, [dhcp_opts]
    mov al, 53
    stosb
    mov al, 1
    stosb
    mov al, 3                   /* Request */
    stosb
    mov al, 50                  /* requested IP */
    stosb
    mov al, 4
    stosb
    lea si, [offered_ip]
    movsw
    movsw
    mov al, 54                  /* server id */
    stosb
    mov al, 4
    stosb
    lea si, [server_id]
    movsw
    movsw
    mov al, 55
    stosb
    mov al, 4
    stosb
    mov al, 1
    stosb
    mov al, 3
    stosb
    mov al, 6
    stosb
    mov al, 51
    stosb
    mov al, 0xFF
    stosb
    mov ax, di
    lea bx, [dhcp_opts]
    sub ax, bx
    mov [dhcp_opt_len], ax
    call assemble_and_tx

    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

assemble_and_tx:
    push ax
    push bx
    push cx
    push dx
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
    mov ax, 0x0008
    stosw

    /* IP */
    mov al, 0x45
    stosb
    xor al, al
    stosb
    /* total length = 20+8+240+opt_len */
    mov ax, [dhcp_opt_len]
    add ax, 20 + 8 + BOOT_LEN
    mov [ip_len], ax
    xchg al, ah
    stosw
    mov ax, [xid]
    xchg al, ah
    stosw
    xor ax, ax
    stosw
    mov al, 64
    stosb
    mov al, 17
    stosb
    xor ax, ax
    stosw
    xor ax, ax
    stosw
    stosw
    mov ax, 0xFFFF
    stosw
    stosw

    /* UDP */
    mov ax, 0x4400
    stosw
    mov ax, 0x4300
    stosw
    mov ax, [dhcp_opt_len]
    add ax, 8 + BOOT_LEN
    xchg al, ah
    stosw
    xor ax, ax
    stosw

    /* BOOTP */
    mov al, 1
    stosb
    mov al, 1
    stosb
    mov al, 6
    stosb
    xor al, al
    stosb
    /* xid big-endian */
    mov al, [xid + 3]
    stosb
    mov al, [xid + 2]
    stosb
    mov al, [xid + 1]
    stosb
    mov al, [xid]
    stosb
    xor ax, ax
    stosw                       /* secs */
    mov ax, 0x0080
    stosw                       /* flags BE 0x8000 stored as LE word 0x0080 on wire? */
    /* On wire flags are big-endian: 80 00. stosw of 0x0080 writes 80 00 — correct. */
    xor ax, ax
    mov cx, 8                   /* ciaddr+yiaddr+siaddr+giaddr = 16 bytes */
    rep stosw
    lea si, [my_mac]
    movsw
    movsw
    movsw
    xor ax, ax
    mov cx, 5
    rep stosw
    mov cx, 192
    xor al, al
    rep stosb
    mov al, 99
    stosb
    mov al, 130
    stosb
    mov al, 83
    stosb
    mov al, 99
    stosb

    lea si, [dhcp_opts]
    mov cx, [dhcp_opt_len]
    rep movsb

    /* IP checksum */
    lea si, [tx_buf + 14]
    mov cx, 20
    call inet_csum
    mov [tx_buf + 24], ah
    mov [tx_buf + 25], al

    /* frame length */
    mov ax, [ip_len]
    add ax, 14
    mov cx, ax
    cmp cx, 60
    jae .tx_go
    lea di, [tx_buf]
    add di, cx
    mov bx, 60
    sub bx, cx
    mov cx, bx
    xor al, al
    rep stosb
    mov cx, 60
.tx_go:
    lea si, [tx_buf]
    call nic_transmit

    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* CF set if not a matching DHCP reply for want_type. */
parse_dhcp_reply:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    cmp word ptr [rx_buf + 12], 0x0008
    jne .pr_bad
    cmp byte ptr [rx_buf + 23], 17
    jne .pr_bad

    mov bl, [rx_buf + 14]
    and bx, 0x000F
    shl bx, 1
    shl bx, 1
    lea si, [rx_buf + 14]
    add si, bx                  /* UDP */
    /* dst port 68 */
    cmp word ptr [si + 2], 0x4400
    jne .pr_bad
    add si, 8                   /* BOOTP */
    cmp byte ptr [si], 2        /* BOOTREPLY */
    jne .pr_bad

    /* xid match (big-endian on wire) */
    mov al, [si + 4]
    cmp al, [xid + 3]
    jne .pr_bad
    mov al, [si + 5]
    cmp al, [xid + 2]
    jne .pr_bad
    mov al, [si + 6]
    cmp al, [xid + 1]
    jne .pr_bad
    mov al, [si + 7]
    cmp al, [xid]
    jne .pr_bad

    /* yiaddr */
    mov ax, [si + 16]
    mov [offered_ip], ax
    mov ax, [si + 18]
    mov [offered_ip + 2], ax

    /* default server id = siaddr */
    mov ax, [si + 20]
    mov [server_id], ax
    mov ax, [si + 22]
    mov [server_id + 2], ax

    /* magic */
    cmp byte ptr [si + 236], 99
    jne .pr_bad
    cmp byte ptr [si + 237], 130
    jne .pr_bad

    lea di, [si + 240]
    call parse_options
    jc .pr_bad

    mov al, [msg_type]
    cmp al, [want_type]
    jne .pr_bad

    clc
    jmp .pr_done
.pr_bad:
    stc
.pr_done:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* DI = options start. Sets msg_type and fills fields. CF on missing type. */
parse_options:
    push ax
    push bx
    push cx
    push si
    mov byte ptr [msg_type], 0
.po_next:
    mov al, [di]
    inc di
    cmp al, 0xFF
    je .po_end
    test al, al
    jz .po_next                 /* pad */
    mov bl, al                  /* code */
    mov cl, [di]
    inc di
    xor ch, ch
    /* DI points at data, CX=len */
    cmp bl, 53
    je .po_53
    cmp bl, 54
    je .po_54
    cmp bl, 1
    je .po_1
    cmp bl, 3
    je .po_3
    cmp bl, 6
    je .po_6
    cmp bl, 51
    je .po_51
.po_skip:
    add di, cx
    jmp .po_next
.po_53:
    test cx, cx
    jz .po_skip
    mov al, [di]
    mov [msg_type], al
    jmp .po_skip
.po_54:
    cmp cx, 4
    jb .po_skip
    mov ax, [di]
    mov [server_id], ax
    mov ax, [di + 2]
    mov [server_id + 2], ax
    jmp .po_skip
.po_1:
    cmp cx, 4
    jb .po_skip
    mov ax, [di]
    mov [subnet_mask], ax
    mov ax, [di + 2]
    mov [subnet_mask + 2], ax
    jmp .po_skip
.po_3:
    cmp cx, 4
    jb .po_skip
    mov ax, [di]
    mov [router_ip], ax
    mov ax, [di + 2]
    mov [router_ip + 2], ax
    jmp .po_skip
.po_6:
    cmp cx, 4
    jb .po_skip
    mov ax, [di]
    mov [dns_ip], ax
    mov ax, [di + 2]
    mov [dns_ip + 2], ax
    jmp .po_skip
.po_51:
    cmp cx, 4
    jb .po_skip
    /* big-endian lease seconds */
    mov al, [di]
    mov [lease_secs + 3], al
    mov al, [di + 1]
    mov [lease_secs + 2], al
    mov al, [di + 2]
    mov [lease_secs + 1], al
    mov al, [di + 3]
    mov [lease_secs], al
    jmp .po_skip
.po_end:
    cmp byte ptr [msg_type], 0
    je .po_bad
    clc
    jmp .po_done
.po_bad:
    stc
.po_done:
    pop si
    pop cx
    pop bx
    pop ax
    ret

/* ---- print ---- */
print_lease:
    push ax
    push dx
    push si
    mov ah, 0x09
    lea dx, [msg_ok]
    int 0x21

    mov ah, 0x09
    lea dx, [msg_ip]
    int 0x21
    lea si, [offered_ip]
    call print_ip
    call print_crlf

    mov ah, 0x09
    lea dx, [msg_mask]
    int 0x21
    lea si, [subnet_mask]
    call print_ip
    call print_crlf

    mov ah, 0x09
    lea dx, [msg_gw]
    int 0x21
    lea si, [router_ip]
    call print_ip
    call print_crlf

    mov ah, 0x09
    lea dx, [msg_dns]
    int 0x21
    lea si, [dns_ip]
    call print_ip
    call print_crlf

    mov ah, 0x09
    lea dx, [msg_server]
    int 0x21
    lea si, [server_id]
    call print_ip
    call print_crlf

    mov ah, 0x09
    lea dx, [msg_lease]
    int 0x21
    call print_u32
    call print_crlf

    pop si
    pop dx
    pop ax
    ret

/* Print [lease_secs] as unsigned 32-bit little-endian. */
print_u32:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov ax, [lease_secs]
    mov dx, [lease_secs + 2]
    lea di, [num_buf + 10]
    mov byte ptr [di], '$'
    mov bx, 10
    mov cx, ax
    or cx, dx
    jnz .u32_loop
    dec di
    mov byte ptr [di], '0'
    jmp .u32_out
.u32_loop:
    push ax
    mov ax, dx
    xor dx, dx
    div bx
    mov si, ax
    pop ax
    div bx
    mov cx, dx
    mov dx, si
    add cl, '0'
    dec di
    mov [di], cl
    mov cx, ax
    or cx, dx
    jnz .u32_loop
.u32_out:
    mov ah, 0x09
    mov dx, di
    int 0x21
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* Persist lease for PING (layout in netlease.inc). */
save_lease:
    push ax
    push si
    push di
    lea di, [lease_buf]
    mov ax, 0x4844
    stosw
    mov ax, 0x5043
    stosw
    mov al, LEASE_VER
    stosb
    xor al, al
    stosb
    stosb
    stosb
    lea si, [offered_ip]
    movsw
    movsw
    lea si, [router_ip]
    movsw
    movsw
    lea si, [subnet_mask]
    movsw
    movsw
    lea si, [dns_ip]
    movsw
    movsw
    call net_save_lease
    pop di
    pop si
    pop ax
    ret

print_crlf:
    push ax
    push dx
    mov ah, 0x02
    mov dl, 13
    int 0x21
    mov dl, 10
    int 0x21
    pop dx
    pop ax
    ret

.include "firmware/src/dos/inc/netlease.inc"
.include "firmware/src/dos/inc/nettsr.inc"
.include "firmware/src/dos/inc/netutil.inc"
.include "firmware/src/dos/inc/ne2000.inc"

msg_help:
    .ascii "Usage: DHCP\r\n"
    .ascii "\r\n"
    .ascii "Acquire an IPv4 lease and write LEASE.DAT for PING.\r\n$"
msg_mode_tsr:
    .ascii "DHCP via TSR

$"
msg_mode_solo:
    .ascii "DHCP solo\r\n$"
msg_start:
    .ascii "\r\nDHCP Client\r\n\r\n$"
msg_discover:
    .ascii "Sending DHCP Discover...\r\n$"
msg_request:
    .ascii "Sending DHCP Request...\r\n$"
msg_ok:
    .ascii "\r\nLease acquired:\r\n$"
msg_ip:
    .ascii "  IP Address. . . . . . . . : $"
msg_mask:
    .ascii "  Subnet Mask . . . . . . . : $"
msg_gw:
    .ascii "  Default Gateway . . . . . : $"
msg_dns:
    .ascii "  DNS Servers . . . . . . . : $"
msg_server:
    .ascii "  DHCP Server . . . . . . . : $"
msg_lease:
    .ascii "  Lease Time (seconds). . . : $"
msg_fail:
    .ascii "\r\nDHCP failed: no lease.\r\n$"
msg_lease_write:
    .ascii "\r\nDHCP: lease acquired but could not write LEASE.DAT.\r\n$"
msg_nic:
    .ascii "\r\nNIC init failed.\r\n$"
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
xid:
    .space 4, 0
offered_ip:
    .byte 0, 0, 0, 0
server_id:
    .byte 0, 0, 0, 0
subnet_mask:
    .byte 0, 0, 0, 0
router_ip:
    .byte 0, 0, 0, 0
dns_ip:
    .byte 0, 0, 0, 0
lease_secs:
    .word 0, 0
msg_type:
    .byte 0
want_type:
    .byte 0
phase:
    .byte 0
abort_flag:
    .byte 0
dhcp_opt_len:
    .word 0
ip_len:
    .word 0
tx_len:
    .word 0
rx_len:
    .word 0
poll_left:
    .word 0
pkt_tries:
    .word 0
dhcp_opts:
    .space OPT_MAX, 0
num_buf:
    .space 12, 0
tx_buf:
    .space 1600, 0
rx_buf:
    .space 1600, 0
