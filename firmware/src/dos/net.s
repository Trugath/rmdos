.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * NET.COM — optional resident NE2000 stack (INT 60h AH=B8h).
 * CONFIG.SYS: INSTALL=A:\BIN\NET.COM  (default image: not installed)
 * Lease lives only in TSR RAM; no LEASE.DAT.
 * NET /U — unload (restore INT 60, free resident PSP).
 */

.set LEASE_SIZE, 24
.set NE_RX_POLL_OUTER, 0x40
.set NET_VER, 2
.set NE2000_FORCE_HW, 1

.include "firmware/src/dos/inc/netlease_defs.inc"

_start:
    jmp install

/* ===================== resident ===================== */

net_int60:
    cmp ah, 0xB8
    jne .n60_chain
    sti
    cmp al, 0
    je .n2f_inst
    cmp al, 1
    je .n2f_mac
    cmp al, 2
    je .n2f_tx
    cmp al, 3
    je .n2f_rx
    cmp al, 4
    je .n2f_getlease
    cmp al, 5
    je .n2f_setlease
    cmp al, 6
    je .n2f_ready
    cmp al, 7
    je .n2f_unload
.n60_iret:
    iret

.n60_chain:
    cmp word ptr cs:[net_old60 + 2], 0
    je .n60_iret
    jmp dword ptr cs:[net_old60]

/* Clear/set CF in the IRET flags frame (clc/stc before iret are lost). */
n60_iret_ok:
    push bp
    mov bp, sp
    and word ptr [bp + 6], 0xFFFE
    pop bp
    iret

n60_iret_cf:
    push bp
    mov bp, sp
    or word ptr [bp + 6], 0x0001
    pop bp
    iret

/* DS must be CS. Sets ES=CS around nic_init (stosb). */
net_ensure_nic:
    cmp byte ptr [nic_ready], 0
    jne .nen_ok
    push es
    push cs
    pop es
    call nic_init
    pop es
    jc .nen_bad
    mov byte ptr [nic_ready], 1
.nen_ok:
    clc
    ret
.nen_bad:
    stc
    ret

.n2f_inst:
    mov al, 0xFF
    mov bx, NET_VER
    jmp n60_iret_ok

.n2f_ready:
    mov al, byte ptr cs:[nic_ready]
    jmp n60_iret_ok

.n2f_unload:
    push ax
    push ds
    push si
    push es
    xor ax, ax
    mov es, ax
    push cs
    pop ds
    mov si, offset net_old60
    mov ax, [si]
    mov word ptr es:[0x60 * 4], ax
    mov ax, [si + 2]
    mov word ptr es:[0x60 * 4 + 2], ax
    pop es
    pop si
    pop ds
    pop ax
    mov bx, cs
    jmp n60_iret_ok

.n2f_mac:
    push ax
    push cx
    push si
    push di
    push ds
    push es
    push cs
    pop ds
    call net_ensure_nic
    jc .n2f_mac_bad
    cld
    lea si, [my_mac]
    mov cx, 6
    rep movsb
    pop es
    pop ds
    pop di
    pop si
    pop cx
    pop ax
    jmp n60_iret_ok
.n2f_mac_bad:
    pop es
    pop ds
    pop di
    pop si
    pop cx
    pop ax
    jmp n60_iret_cf

.n2f_tx:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    jcxz .n2f_tx_bad
    cmp cx, 1600
    ja .n2f_tx_bad
    push cs
    pop es
    cld
    mov word ptr cs:[tx_len], cx
    lea di, [tx_buf]
    rep movsb
    push cs
    pop ds
    call net_ensure_nic
    jc .n2f_tx_bad
    mov cx, [tx_len]
    lea si, [tx_buf]
    call nic_transmit
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    jmp n60_iret_ok
.n2f_tx_bad:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    jmp n60_iret_cf

.n2f_rx:
    push ax
    push bx
    push dx
    push si
    push di
    push ds
    push es
    /* caller BX=max, ES:DI=dest */
    push bx
    push di
    push es
    push cs
    pop ds
    push cs
    pop es
    call net_ensure_nic
    jc .n2f_rx_fail
    cld
    call nic_rx
    pop es
    pop di
    pop bx
    jc .n2f_rx_none
    cmp cx, bx
    jbe .n2f_rx_copy
    mov cx, bx
.n2f_rx_copy:
    push cx
    lea si, [rx_buf]
    rep movsb
    pop cx
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop bx
    pop ax
    jmp n60_iret_ok
.n2f_rx_fail:
    pop es
    pop di
    pop bx
.n2f_rx_none:
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop bx
    pop ax
    jmp n60_iret_cf

.n2f_getlease:
    push ax
    push cx
    push si
    push ds
    push cs
    pop ds
    cld
    cmp word ptr [lease_buf], 0x4844
    jne .n2f_gl_bad
    cmp word ptr [lease_buf + 2], 0x5043
    jne .n2f_gl_bad
    cmp byte ptr [lease_buf + LEASE_OFF_VER], LEASE_VER
    jne .n2f_gl_bad
    lea si, [lease_buf]
    mov cx, LEASE_SIZE
    rep movsb
    pop ds
    pop si
    pop cx
    pop ax
    jmp n60_iret_ok
.n2f_gl_bad:
    pop ds
    pop si
    pop cx
    pop ax
    jmp n60_iret_cf

.n2f_setlease:
    push ax
    push cx
    push di
    push es
    push ds
    push cs
    pop es
    cld
    lea di, [lease_buf]
    mov cx, LEASE_SIZE
    rep movsb
    pop ds
    pop es
    pop di
    pop cx
    pop ax
    jmp n60_iret_ok

net_old60:
    .word 0, 0
net_use_tsr:
    .byte 0
nic_ready:
    .byte 0
my_mac:
    .space 6, 0
tx_len:
    .word 0
rx_len:
    .word 0
poll_left:
    .word 0
lease_buf:
    .space LEASE_SIZE, 0
tx_buf:
    .space 1600, 0
rx_buf:
    .space 1600, 0

check_abort:
    xor ax, ax
    ret

.include "firmware/src/dos/inc/ne2000.inc"

    .align 16
    .space 16, 0
tsr_end:

/* ===================== transient installer / unload ===================== */

install:
    push cs
    pop ds
    push cs
    pop es

    /* Command tail: "/U" → unload */
    mov si, 0x81
    mov cl, byte ptr [0x80]
    xor ch, ch
    jcxz .inst_check
.inst_skip:
    lodsb
    cmp al, ' '
    je .inst_skip_sp
    cmp al, 9
    je .inst_skip_sp
    cmp al, '/'
    je .inst_slash
    cmp al, '-'
    je .inst_slash
    jmp .inst_check
.inst_skip_sp:
    loop .inst_skip
    jmp .inst_check
.inst_slash:
    lodsb
    and al, 0xDF
    cmp al, 'U'
    je do_unload

.inst_check:
    push es
    xor ax, ax
    mov es, ax
    cmp word ptr es:[0x60 * 4 + 2], 0
    pop es
    je .do_install
    mov ax, 0xB800
    int 0x60
    cmp al, 0xFF
    jne .do_install
    mov ah, 0x09
    lea dx, [msg_already]
    int 0x21
    mov ax, 0x4C01
    int 0x21

.do_install:
    mov byte ptr [net_use_tsr], 0
    mov byte ptr [nic_ready], 0
    /* NIC inited lazily on first B8 MAC/TX/RX. */

    push es
    xor ax, ax
    mov es, ax
    mov ax, word ptr es:[0x60 * 4]
    mov word ptr [net_old60], ax
    mov ax, word ptr es:[0x60 * 4 + 2]
    mov word ptr [net_old60 + 2], ax
    pop es

    mov ax, 0x2560
    lea dx, [net_int60]
    int 0x21

    mov ah, 0x09
    lea dx, [msg_ok]
    int 0x21

    mov ax, word ptr [0x2C]
    test ax, ax
    jz .inst_keep
    mov es, ax
    mov ah, 0x49
    int 0x21
    mov word ptr [0x2C], 0

.inst_keep:
    lea dx, [tsr_end]
    add dx, 15
    mov cl, 4
    shr dx, cl
    mov ax, 0x3100
    int 0x21

.fail:
    mov ah, 0x09
    lea dx, [msg_fail]
    int 0x21
    mov ax, 0x4C01
    int 0x21

do_unload:
    push es
    xor ax, ax
    mov es, ax
    cmp word ptr es:[0x60 * 4 + 2], 0
    pop es
    je .ul_none
    mov ax, 0xB800
    int 0x60
    cmp al, 0xFF
    jne .ul_none
    cmp bx, NET_VER
    jne .ul_ver
    mov ax, 0xB807
    int 0x60
    mov es, bx
    mov ah, 0x49
    int 0x21
    jc .ul_fail
    mov ah, 0x09
    lea dx, [msg_unloaded]
    int 0x21
    mov ax, 0x4C00
    int 0x21
.ul_none:
    mov ah, 0x09
    lea dx, [msg_notloaded]
    int 0x21
    mov ax, 0x4C01
    int 0x21
.ul_ver:
    mov ah, 0x09
    lea dx, [msg_ulver]
    int 0x21
    mov ax, 0x4C01
    int 0x21
.ul_fail:
    mov ah, 0x09
    lea dx, [msg_ulfail]
    int 0x21
    mov ax, 0x4C01
    int 0x21

msg_ok:
    .ascii "NET resident\r\n$"
msg_already:
    .ascii "NET already loaded\r\n$"
msg_fail:
    .ascii "NET init failed\r\n$"
msg_unloaded:
    .ascii "NET unloaded\r\n$"
msg_notloaded:
    .ascii "NET not loaded\r\n$"
msg_ulver:
    .ascii "NET version mismatch\r\n$"
msg_ulfail:
    .ascii "NET unload failed\r\n$"
