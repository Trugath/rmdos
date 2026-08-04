.code16
.intel_syntax noprefix

.include "firmware/bios/inc/equates.inc"

/*
 * rmDOS clean-room XT system BIOS — cold-boot initialisation:
 * PIC/PIT programming, BDA setup, IVT fill, memory sizing, option-ROM scan.
 */

.section .text
.global init_pic, init_pit, init_bda, init_ivt
.global size_memory, scan_option_roms
.global probe_ports

init_pic:
    mov al, 0x13
    out PORT_PIC_CMD, al
    mov al, 0x08
    out PORT_PIC_DATA, al
    mov al, 0x09
    out PORT_PIC_DATA, al
    mov al, 0xBC                /* mask all except IRQ0+IRQ1+IRQ6 */
    out PORT_PIC_DATA, al
    ret

init_pit:
    mov al, 0x36
    out PORT_PIT_MODE, al
    xor al, al
    out PORT_PIT_CH0, al
    out PORT_PIT_CH0, al
    ret

init_bda:
    push es
    mov ax, BDA_SEG
    mov es, ax
    push word ptr es:[BDA_WARM_FLAG]

    xor di, di
    mov cx, 0x80
    xor ax, ax
    rep stosw

    pop ax                      /* prior warm flag */
    /* COM/LPT bases filled by probe_ports; leave zero until then */
    push ax
    call read_sw1
    mov es:[BDA_EQUIP], ax
    pop ax

    mov word ptr es:[BDA_KBD_BUF_HEAD], BDA_KBD_BUF
    mov word ptr es:[BDA_KBD_BUF_TAIL], BDA_KBD_BUF
    mov word ptr es:[BDA_KBD_BUF_START], BDA_KBD_BUF
    mov word ptr es:[BDA_KBD_BUF_ENDPTR], BDA_KBD_BUF_END

    mov byte ptr es:[BDA_CRT_MODE], 0x03
    mov word ptr es:[BDA_CRT_COLS], 80
    mov word ptr es:[BDA_CRT_LEN], 0x1000
    mov word ptr es:[BDA_CRT_START], 0
    mov word ptr es:[BDA_CURSOR_TYPE], 0x0607
    mov byte ptr es:[BDA_CRT_PAGE], 0
    mov word ptr es:[BDA_CRT_PORT], 0x3D4
    mov byte ptr es:[BDA_CRT_MODE_REG], 0x29
    mov byte ptr es:[BDA_CRT_PALETTE], 0x07
    mov byte ptr es:[BDA_ROWS], 24
    mov word ptr es:[BDA_MEMKB], 640

    cmp ax, WARM_BOOT_MAGIC
    jne .bda_probe
    mov word ptr es:[BDA_WARM_FLAG], WARM_BOOT_MAGIC
.bda_probe:
    call probe_ports
    pop es
    ret

/*
 * Probe classic COM (3F8/2F8) and LPT (378/278) bases via scratch/data
 * readback. Fill BDA 40:00–40:0C and equipment-word serial/parallel counts.
 * DS/ES = BDA on entry not required; uses ES=BDA.
 */
probe_ports:
    push ax
    push bx
    push cx
    push dx
    push es
    push si
    push di
    mov ax, BDA_SEG
    mov es, ax

    xor ax, ax
    mov word ptr es:[BDA_COM1], ax
    mov word ptr es:[BDA_COM2], ax
    mov word ptr es:[BDA_COM3], ax
    mov word ptr es:[BDA_COM4], ax
    mov word ptr es:[BDA_LPT1], ax
    mov word ptr es:[BDA_LPT2], ax
    mov word ptr es:[BDA_LPT3], ax

    xor bx, bx                   /* BX = COM slot index 0..1 */
    mov si, 0x3F8
    call .probe_com
    mov si, 0x2F8
    call .probe_com

    xor bx, bx                   /* BX = LPT slot index 0..1 */
    mov si, 0x378
    call .probe_lpt
    mov si, 0x278
    call .probe_lpt

    /* equipment: bits 9-11 = serial count, bits 14-15 = parallel count */
    mov ax, es:[BDA_EQUIP]
    and ax, 0x31FF               /* clear serial (9-11) + parallel (14-15) counts */
    xor cx, cx
    cmp word ptr es:[BDA_COM1], 0
    je .pc1
    inc cx
.pc1:
    cmp word ptr es:[BDA_COM2], 0
    je .pc2
    inc cx
.pc2:
    mov bx, cx
    mov cl, 9
    shl bx, cl
    or ax, bx
    xor cx, cx
    cmp word ptr es:[BDA_LPT1], 0
    je .pl1
    inc cx
.pl1:
    cmp word ptr es:[BDA_LPT2], 0
    je .pl2
    inc cx
.pl2:
    mov bx, cx
    mov cl, 14
    shl bx, cl
    or ax, bx
    mov es:[BDA_EQUIP], ax

    pop di
    pop si
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret

.probe_com:
    /* SI = candidate base; BX = next COM slot (0 or 1). Scratch at base+7. */
    push ax
    push dx
    mov dx, si
    add dx, 7
    mov al, 0x5A
    out dx, al
    in al, dx
    cmp al, 0x5A
    jne .pcom_miss
    mov al, 0xA5
    out dx, al
    in al, dx
    cmp al, 0xA5
    jne .pcom_miss
    cmp bx, 2
    jae .pcom_miss
    mov di, bx
    shl di, 1
    mov es:[di], si              /* BDA_COM1 + slot*2 */
    inc bx
.pcom_miss:
    pop dx
    pop ax
    ret

.probe_lpt:
    /* SI = candidate base; BX = next LPT slot. Data latch readback. */
    push ax
    push dx
    mov dx, si
    mov al, 0xA5
    out dx, al
    in al, dx
    cmp al, 0xA5
    jne .plpt_miss
    mov al, 0x5A
    out dx, al
    in al, dx
    cmp al, 0x5A
    jne .plpt_miss
    cmp bx, 2
    jae .plpt_miss
    mov di, bx
    shl di, 1
    add di, BDA_LPT1
    mov es:[di], si
    inc bx
.plpt_miss:
    pop dx
    pop ax
    ret

read_sw1:
    push dx
    in al, PORT_PPI_B
    mov ah, al
    or al, 0x80
    out PORT_PPI_B, al
    in al, PORT_PPI_A
    mov dl, al
    mov al, ah
    out PORT_PPI_B, al
    mov al, dl
    xor ah, ah
    pop dx
    ret

init_ivt:
    push es
    push ds
    xor ax, ax
    mov es, ax
    mov ds, ax

    xor di, di
    mov cx, 256
.fill_ivt:
    mov ax, offset isr_default
    stosw
    mov ax, BIOS_SEG
    stosw
    loop .fill_ivt

    mov word ptr [0x08 * 4], offset isr_08
    mov word ptr [0x08 * 4 + 2], BIOS_SEG
    mov word ptr [0x09 * 4], offset isr_09
    mov word ptr [0x09 * 4 + 2], BIOS_SEG
    mov word ptr [0x05 * 4], offset int5_handler
    mov word ptr [0x05 * 4 + 2], BIOS_SEG
    mov word ptr [0x0E * 4], offset isr_0e
    mov word ptr [0x0E * 4 + 2], BIOS_SEG
    mov word ptr [0x10 * 4], 0xF065
    mov word ptr [0x10 * 4 + 2], BIOS_SEG
    mov word ptr [0x11 * 4], offset int11_handler
    mov word ptr [0x11 * 4 + 2], BIOS_SEG
    mov word ptr [0x12 * 4], offset int12_handler
    mov word ptr [0x12 * 4 + 2], BIOS_SEG
    mov word ptr [0x13 * 4], offset int13_handler
    mov word ptr [0x13 * 4 + 2], BIOS_SEG
    mov word ptr [0x14 * 4], offset int14_handler
    mov word ptr [0x14 * 4 + 2], BIOS_SEG
    mov word ptr [0x15 * 4], offset int15_handler
    mov word ptr [0x15 * 4 + 2], BIOS_SEG
    mov word ptr [0x16 * 4], offset int16_handler
    mov word ptr [0x16 * 4 + 2], BIOS_SEG
    mov word ptr [0x17 * 4], offset int17_handler
    mov word ptr [0x17 * 4 + 2], BIOS_SEG
    mov word ptr [0x18 * 4], offset int18_handler
    mov word ptr [0x18 * 4 + 2], BIOS_SEG
    mov word ptr [0x19 * 4], offset int19_handler
    mov word ptr [0x19 * 4 + 2], BIOS_SEG
    mov word ptr [0x1A * 4], offset int1a_handler
    mov word ptr [0x1A * 4 + 2], BIOS_SEG
    mov word ptr [0x1E * 4], offset disk_base_table
    mov word ptr [0x1E * 4 + 2], BIOS_SEG
    mov word ptr [0x1F * 4], offset bios_font_hi
    mov word ptr [0x1F * 4 + 2], BIOS_SEG
    /* INT 33h — safe absent-mouse IRET (MOUSE.COM replaces when loaded) */
    mov word ptr [0x33 * 4], offset int33_stub
    mov word ptr [0x33 * 4 + 2], BIOS_SEG

    pop ds
    pop es
    ret

size_memory:
    push es
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov ax, [BDA_WARM_FLAG]
    cmp ax, WARM_BOOT_MAGIC
    je .size_skip

    mov bx, 0x0400
.size_loop:
    cmp bx, 0xA000
    jae .size_done
    mov es, bx
    mov word ptr es:[0], 0xAA55
    cmp word ptr es:[0], 0xAA55
    jne .size_done
    mov word ptr es:[0], 0x55AA
    cmp word ptr es:[0], 0x55AA
    jne .size_done
    mov word ptr es:[0], 0
    add bx, 0x40
    jmp .size_loop
.size_done:
    mov ax, bx
    mov cl, 6
    shr ax, cl
    mov [BDA_MEMKB], ax
.size_skip:
    pop ds
    pop es
    ret

scan_option_roms:
    push ds
    mov ax, 0xC000
.scan_loop:
    cmp ax, 0xF400
    jae .scan_done
    mov ds, ax
    cmp word ptr [0], 0xAA55
    jne .scan_next
    mov cl, [2]
    xor ch, ch
    test cx, cx
    jz .scan_next
    push ax
    xor si, si
    xor dx, dx
    mov bx, cx
    mov cl, 9
    shl bx, cl
    mov cx, bx
.sum_loop:
    lodsb
    add dl, al
    loop .sum_loop
    test dl, dl
    pop ax
    jnz .scan_next
    /* far call DS:0003 */
    push ax
    push cs
    mov bx, offset .rom_ret
    push bx
    push ds
    mov bx, 3
    push bx
    retf
.rom_ret:
    pop ax
.scan_next:
    add ax, 0x80
    jmp .scan_loop
.scan_done:
    pop ds
    ret
