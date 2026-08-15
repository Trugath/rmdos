/*
 * rmDOS resident NET multiplex via INT 60h (AH=B8h).
 * Caller must provide: net_use_tsr (.byte), and for lease helpers the
 * usual lease_buf / lease_path from netlease.inc.
 *
 * AL functions (AH=B8h):
 *   0 install check → AL=FFh, BX=version
 *   1 get MAC → ES:DI (6 bytes)
 *   2 transmit → DS:SI frame, CX=len; CF=error
 *   3 receive → ES:DI buf, BX=max; CF=none else CX=len
 *   4 get lease → ES:DI (24 bytes); CF=invalid
 *   5 set lease → DS:SI (24 bytes)
 *   6 nic ready → AL=1 if NIC inited
 *   7 prepare unload → restore INT 60; BX=TSR PSP
 */

.set NET_MUX_AH, 0xB8
.set NET_INT, 0x60
.set NET_VER, 2

/* Probe TSR. Sets net_use_tsr=1 if present. Never calls a null INT 60. */
net_probe:
    push ax
    push bx
    push es
    mov byte ptr [net_use_tsr], 0
    xor ax, ax
    mov es, ax
    cmp word ptr es:[NET_INT * 4 + 2], 0
    je .np_done
    mov ax, (NET_MUX_AH << 8)
    int NET_INT
    cmp al, 0xFF
    jne .np_done
    mov byte ptr [net_use_tsr], 1
.np_done:
    pop es
    pop bx
    pop ax
    ret

/* Read lease: TSR if present, else LEASE.DAT. CF=fail. */
net_load_lease:
    cmp byte ptr [net_use_tsr], 0
    je lease_read_file
    push ax
    push bx
    push es
    push di
    push ds
    pop es
    lea di, [lease_buf]
    mov ax, 0xB804
    int NET_INT
    pop di
    pop es
    pop bx
    pop ax
    ret

/* Write lease: TSR if present, else LEASE.DAT. CF=fail. */
net_save_lease:
    cmp byte ptr [net_use_tsr], 0
    je lease_write_file
    push ax
    push si
    lea si, [lease_buf]
    mov ax, 0xB805
    int NET_INT
    pop si
    pop ax
    ret
