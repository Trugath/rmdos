.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * NETTEST.COM — mux self-check when NET.COM is resident.
 * Probe B8, get MAC, TX a minimal broadcast, RX poll once.
 */

.set NE_RX_POLL_OUTER, 0x10
.set NET_VER, 2

_start:
    push cs
    pop ds
    push cs
    pop es

    call net_probe
    cmp byte ptr [net_use_tsr], 0
    je .fail_probe

    mov ax, 0xB800
    int 0x60
    cmp al, 0xFF
    jne .fail_probe
    cmp bx, NET_VER
    jne .fail_probe

    call nic_init
    jc .fail_mac
    /* MAC must not be all zeros. */
    lea si, [my_mac]
    mov cx, 6
    xor al, al
.mac_chk:
    or al, [si]
    inc si
    loop .mac_chk
    test al, al
    jz .fail_mac

    /* Minimal Ethernet broadcast (60-byte pad). */
    lea di, [tx_buf]
    mov ax, 0xFFFF
    stosw
    stosw
    stosw
    lea si, [my_mac]
    movsw
    movsw
    movsw
    mov ax, 0x0008                   /* ethertype 0x0800 */
    stosw
    mov cx, 46
    xor al, al
    rep stosb
    lea si, [tx_buf]
    mov cx, 60
    call nic_transmit
    jc .fail_tx

    call nic_rx                       /* CF ok — may be empty */

    mov ah, 0x09
    lea dx, [msg_ok]
    int 0x21
    mov ax, 0x4C00
    int 0x21

.fail_probe:
    mov ah, 0x09
    lea dx, [msg_noprobe]
    int 0x21
    mov ax, 0x4C01
    int 0x21

.fail_mac:
    mov ah, 0x09
    lea dx, [msg_mac]
    int 0x21
    mov ax, 0x4C01
    int 0x21

.fail_tx:
    mov ah, 0x09
    lea dx, [msg_tx]
    int 0x21
    mov ax, 0x4C01
    int 0x21

/* Same IVT guard as nettsr.inc net_probe. */
net_probe:
    push ax
    push bx
    push es
    mov byte ptr [net_use_tsr], 0
    xor ax, ax
    mov es, ax
    cmp word ptr es:[0x60 * 4 + 2], 0
    je .np_done
    mov ax, 0xB800
    int 0x60
    cmp al, 0xFF
    jne .np_done
    mov byte ptr [net_use_tsr], 1
.np_done:
    pop es
    pop bx
    pop ax
    ret

check_abort:
    xor ax, ax
    ret

.include "firmware/src/dos/inc/ne2000.inc"

net_use_tsr:
    .byte 0
my_mac:
    .space 6, 0
tx_len:
    .word 0
rx_len:
    .word 0
poll_left:
    .word 0
tx_buf:
    .space 1600, 0
rx_buf:
    .space 1600, 0

msg_ok:
    .ascii "NETTEST OK\r\n$"
msg_noprobe:
    .ascii "NETTEST: no TSR\r\n$"
msg_mac:
    .ascii "NETTEST: bad MAC\r\n$"
msg_tx:
    .ascii "NETTEST: TX fail\r\n$"
