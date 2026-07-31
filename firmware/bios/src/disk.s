.code16
.intel_syntax noprefix

.include "firmware/bios/inc/equates.inc"

/*
 * rmDOS clean-room XT system BIOS — disk:
 * INT 13h services, INT 19h bootstrap loader, diskette parameter table.
 */

.section .text
.global int13_handler, int19_handler, disk_base_table

int13_handler:
    sti
    cmp dl, 0x80
    jae .i13_hd_fail
    cmp ah, 0x00
    je .i13_ok
    cmp ah, 0x01
    je .i13_ok
    cmp ah, 0x08
    je .i13_params
    mov ah, 0x01
    stc
    jmp .i13_ret
.i13_params:
    /* 720 KB: 80 cyl × 2 heads × 9 spt (matches k8086 geometryFor + bt_disk). */
    xor ax, ax
    mov bx, 0x0003              /* BL = 03h 720K drive type */
    mov cx, 0x4F09              /* CH=79 max cyl, CL=9 SPT */
    mov dx, 0x0101              /* DH=1 max head, DL=1 drive */
.i13_ok:
    xor ah, ah
    clc
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
.i13_hd_fail:
    mov ah, 0x01
    stc
    jmp .i13_ret

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
    jc .i19_fail

    cmp word ptr [BOOT_OFF + 510], 0xAA55
    jne .i19_fail

    mov dl, 0x00
    jmp 0x0000:BOOT_OFF

.i19_fail:
    int 0x18
    jmp .i19_fail

disk_base_table:
    .byte 0xCF, 0x02, 0x25, 0x02, 0x08, 0x2A, 0xFF, 0x50, 0xF6, 0x0F, 0x08
