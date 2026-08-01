.code16
.intel_syntax noprefix

.include "firmware/bios/inc/equates.inc"

/*
 * rmDOS clean-room XT system BIOS — disk:
 * INT 13h floppy via FDC helpers, INT 19h bootstrap, diskette parameter table.
 */

.section .text
.global int13_handler, int19_handler, disk_base_table

int13_handler:
    sti
    cmp dl, 0x80
    jae .i13_hd_fail
    cmp ah, 0x00
    je .i13_reset
    cmp ah, 0x01
    je .i13_status
    cmp ah, 0x02
    je .i13_read
    cmp ah, 0x03
    je .i13_write
    cmp ah, 0x04
    je .i13_verify
    cmp ah, 0x05
    je .i13_format
    cmp ah, 0x08
    je .i13_params
    cmp ah, 0x15
    je .i13_dasd
    cmp ah, 0x16
    je .i13_change
    mov ah, 0x01
    stc
    jmp .i13_ret

.i13_hd_fail:
    mov ah, 0x01
    stc
    jmp .i13_ret

.i13_reset:
    push bx
    push cx
    push dx
    call fdc_reset
    /* Recalibrate drive 0 (and 1 if equipment says so) */
    xor dl, dl
    xor ch, ch
    call fdc_seek
    pop dx
    pop cx
    pop bx
    xor ah, ah
    call fdc_store_status
    jmp .i13_ret

.i13_status:
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov ah, [BDA_FLOPPY_STATUS]
    pop ds
    test ah, ah
    jz .i13_st_ok
    stc
    jmp .i13_ret
.i13_st_ok:
    clc
    jmp .i13_ret

.i13_read:
    mov ah, 0
    call fdc_do_rw
    call fdc_store_status
    pushf
    cmp ch, 40
    jb .i13_read_done
    call disk_select_720
.i13_read_done:
    popf
    jmp .i13_ret

.i13_write:
    mov ah, 1
    call fdc_do_rw
    call fdc_store_status
    jmp .i13_ret

.i13_verify:
    mov ah, 2
    call fdc_do_rw
    call fdc_store_status
    jmp .i13_ret

.i13_format:
    call fdc_do_format
    call fdc_store_status
    jmp .i13_ret

.i13_params:
    /* Report 360K or 720K from last media / host hint (BDA 40:8B). */
    push ds
    push ax
    mov ax, BDA_SEG
    mov ds, ax
    mov al, [0x8B]
    pop ax
    pop ds
    cmp al, 1
    je .i13_params_360
    /* 720 KB: 80 cyl × 2 heads × 9 spt */
    xor ax, ax
    mov bx, 0x0003
    mov cx, 0x4F09
    mov dx, 0x0101
    xor ah, ah
    clc
    jmp .i13_ret
.i13_params_360:
    /* 360 KB: 40 cyl × 2 heads × 9 spt */
    call disk_select_360
    xor ax, ax
    mov bx, 0x0001
    mov cx, 0x2709
    mov dx, 0x0101
    xor ah, ah
    clc
    jmp .i13_ret

.i13_dasd:
    /* AH=15: diskette with change-line support → AH=02 */
    cmp dl, 1
    ja .i13_dasd_none
    mov ah, 0x02
    clc
    jmp .i13_ret
.i13_dasd_none:
    mov ah, 0
    stc
    jmp .i13_ret

.i13_change:
    /* AH=16: change-line active → AH=06, CF */
    push dx
    call fdc_read_dir
    pop dx
    test al, 0x80
    jz .i13_nochg
    mov ah, 0x06
    stc
    jmp .i13_ret
.i13_nochg:
    xor ah, ah
    clc
    jmp .i13_ret

.i13_ret:
    push bp
    mov bp, sp
    jc .i13_set_cf
    and word ptr [bp + 6], 0xFFFE
    jmp .i13_done_cf
.i13_set_cf:
    or word ptr [bp + 6], 0x0001
.i13_done_cf:
    pop bp
    iret

int19_handler:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov ah, 0x00
    mov dl, 0x00
    int 0x13

    mov ax, 0x0201
    mov bx, BOOT_OFF
    mov cx, 0x0001
    mov dx, 0x0000
    int 0x13
    jc .i19_hd

    cmp word ptr [BOOT_OFF + 510], 0xAA55
    jne .i19_hd

    mov dl, 0x00
    jmp 0x0000:BOOT_OFF

.i19_hd:
    mov ah, 0x00
    mov dl, 0x80
    int 0x13
    mov ax, 0x0201
    mov bx, BOOT_OFF
    mov cx, 0x0001
    mov dx, 0x0080
    int 0x13
    jc .i19_fail
    cmp word ptr [BOOT_OFF + 510], 0xAA55
    jne .i19_fail
    mov dl, 0x80
    jmp 0x0000:BOOT_OFF

.i19_fail:
    int 0x18
    jmp .i19_fail

/* INT 1Eh — 720K DD diskette parameter table (default) */
disk_base_table:
    .byte 0xAF, 0x02             /* Specify: SRT/HUT, HLT/ND */
    .byte 0x25                   /* motor off delay (ticks) */
    .byte 0x02                   /* N = 512 */
    .byte 0x09                   /* EOT / sectors per track */
    .byte 0x2A                   /* GPL */
    .byte 0xFF                   /* DTL */
    .byte 0x50                   /* format gap */
    .byte 0xF6                   /* format fill */
    .byte 0x0F                   /* head settle (ms) */
    .byte 0x00                   /* motor start (ticks); 0 = no wait (emulator) */

/* INT 1Eh — 360K DD table (same SPT/N; milder specify) */
disk_base_table_360:
    .byte 0xCF, 0x02
    .byte 0x25
    .byte 0x02
    .byte 0x09
    .byte 0x2A
    .byte 0xFF
    .byte 0x50
    .byte 0xF6
    .byte 0x0F
    .byte 0x00

/* Repoint INT 1Eh to 360K table (idempotent). */
disk_select_360:
    push ax
    push ds
    xor ax, ax
    mov ds, ax
    mov word ptr [0x1E * 4], offset disk_base_table_360
    mov word ptr [0x1E * 4 + 2], cs
    mov ax, BDA_SEG
    mov ds, ax
    mov byte ptr [0x8B], 1
    pop ds
    pop ax
    ret

/* Repoint INT 1Eh to 720K table. */
disk_select_720:
    push ax
    push ds
    xor ax, ax
    mov ds, ax
    mov word ptr [0x1E * 4], offset disk_base_table
    mov word ptr [0x1E * 4 + 2], cs
    mov ax, BDA_SEG
    mov ds, ax
    mov byte ptr [0x8B], 3
    pop ds
    pop ax
    ret
