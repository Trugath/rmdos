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
    cmp ah, 0x17
    je .i13_set_dasd
    cmp ah, 0x18
    je .i13_set_media
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
    /* Upgrade 360K → 720K when guest seeks past 40 cyl (same as classic). */
    cmp ch, 40
    jb .i13_read_done
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov al, [0x8B]
    pop ds
    cmp al, 1
    jne .i13_read_done
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
    /* Report media from BDA 40:8B (host image-size hint / last select). */
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov al, [0x8B]
    pop ds
    cmp al, 1
    je .i13_params_360
    cmp al, 2
    je .i13_params_1200
    cmp al, 4
    je .i13_params_1440
    /* 720 KB DD (type 3 or default): 80 cyl × 2 heads × 9 spt */
    call disk_select_720
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
.i13_params_1200:
    /* 1.2 MB 5.25" HD: 80 cyl × 2 heads × 15 spt */
    call disk_select_1200
    xor ax, ax
    mov bx, 0x0002
    mov cx, 0x4F0F
    mov dx, 0x0101
    xor ah, ah
    clc
    jmp .i13_ret
.i13_params_1440:
    /* 1.44 MB 3.5" HD: 80 cyl × 2 heads × 18 spt */
    call disk_select_1440
    xor ax, ax
    mov bx, 0x0004
    mov cx, 0x4F12
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

.i13_set_dasd:
    /* AH=17h set DASD type: AL=type for drive DL; store BDA 40:8C+DL */
    cmp dl, 1
    ja .i13_sd_bad
    push ds
    mov bx, BDA_SEG
    mov ds, bx
    mov bx, 0x8C
    add bl, dl
    mov [bx], al
    pop ds
    xor ah, ah
    clc
    jmp .i13_ret
.i13_sd_bad:
    mov ah, 0x01
    stc
    jmp .i13_ret

.i13_set_media:
    /* AH=18h set media for format: CH=max track, CL=SPT → select table */
    cmp dl, 1
    ja .i13_sm_bad
    cmp cl, 18
    jae .i13_sm_144
    cmp cl, 15
    jae .i13_sm_120
    cmp ch, 79
    jae .i13_sm_720
    call disk_select_360
    jmp .i13_sm_ok
.i13_sm_720:
    call disk_select_720
    jmp .i13_sm_ok
.i13_sm_120:
    call disk_select_1200
    jmp .i13_sm_ok
.i13_sm_144:
    call disk_select_1440
.i13_sm_ok:
    push ds
    xor ax, ax
    mov ds, ax
    les di, dword ptr [0x1E * 4]
    pop ds
    xor ah, ah
    clc
    jmp .i13_ret
.i13_sm_bad:
    mov ah, 0x01
    stc
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

/* INT 1Eh — 1.44M 3.5" HD (18 SPT) */
disk_base_table_1440:
    .byte 0xAF, 0x02
    .byte 0x25
    .byte 0x02
    .byte 0x12                   /* EOT = 18 */
    .byte 0x1B                   /* GPL */
    .byte 0xFF
    .byte 0x54                   /* format gap */
    .byte 0xF6
    .byte 0x0F
    .byte 0x00

/* INT 1Eh — 1.2M 5.25" HD (15 SPT) */
disk_base_table_1200:
    .byte 0xDF, 0x02
    .byte 0x25
    .byte 0x02
    .byte 0x0F                   /* EOT = 15 */
    .byte 0x1B
    .byte 0xFF
    .byte 0x54
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

/* Repoint INT 1Eh to 1.44M table. */
disk_select_1440:
    push ax
    push ds
    xor ax, ax
    mov ds, ax
    mov word ptr [0x1E * 4], offset disk_base_table_1440
    mov word ptr [0x1E * 4 + 2], cs
    mov ax, BDA_SEG
    mov ds, ax
    mov byte ptr [0x8B], 4
    pop ds
    pop ax
    ret

/* Repoint INT 1Eh to 1.2M table. */
disk_select_1200:
    push ax
    push ds
    xor ax, ax
    mov ds, ax
    mov word ptr [0x1E * 4], offset disk_base_table_1200
    mov word ptr [0x1E * 4 + 2], cs
    mov ax, BDA_SEG
    mov ds, ax
    mov byte ptr [0x8B], 2
    pop ds
    pop ax
    ret
