.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * rmDOS FAT12 bootstrap (adapted from WispOS boot_fat12.s).
 *
 * BIOS loads us at 0000:7C00. Bytes 3..61 are overwritten with a FAT12 BPB by
 * mkfs_fat12. Sector 1 is an RFAT1 loader info block with KERNEL.SYS LBA + count.
 * We load KERNEL.SYS into 0070:0000 and far-jump there.
 *
 * Short conditional jumps only (GAS 386 long Jcc is POP CS on 8088).
 * No INT 10h in the boot path.
 */

.equ KERNEL_SEGMENT, 0x0070
.equ FLOPPY_SECTORS_PER_TRACK, 9
.equ FLOPPY_HEADS, 2
.equ LOADER_BUFFER, 0x0500
.equ LOADER_BOOT_KERNEL_START, 0x1c
.equ LOADER_BOOT_KERNEL_SECTORS, 0x1e

_start:
    jmp boot_start
    nop
    .space 59, 0

boot_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    mov [boot_drive], dl

    xor ax, ax
    mov es, ax
    mov bx, LOADER_BUFFER
    mov ax, 1
    call read_lba_sector
    jnc .loader_ok
    jmp hang
.loader_ok:

    cmp byte ptr [LOADER_BUFFER], 'R'
    jne hang
    cmp byte ptr [LOADER_BUFFER + 1], 'F'
    jne hang
    cmp byte ptr [LOADER_BUFFER + 2], 'A'
    jne hang
    cmp byte ptr [LOADER_BUFFER + 3], 'T'
    jne hang
    cmp byte ptr [LOADER_BUFFER + 4], '1'
    jne hang

    mov ax, [LOADER_BUFFER + LOADER_BOOT_KERNEL_START]
    mov [kernel_lba], ax
    mov cx, [LOADER_BUFFER + LOADER_BOOT_KERNEL_SECTORS]
    mov [kernel_sectors], cx

    mov ax, KERNEL_SEGMENT
    mov es, ax
    xor bx, bx
    mov ax, [kernel_lba]
    mov cx, [kernel_sectors]

.load_kernel_loop:
    test cx, cx
    jz .jump_to_kernel
    call read_lba_sector
    jnc .kernel_sector_ok
    jmp hang
.kernel_sector_ok:
    inc ax
    add bx, 512
    dec cx
    jmp .load_kernel_loop

.jump_to_kernel:
    mov dl, [boot_drive]
    sti
    jmp KERNEL_SEGMENT, 0x0000

hang:
    hlt
    jmp hang

/*
 * Read one 512-byte sector.
 * IN:  AX = LBA, ES:BX = buffer, DS:boot_drive = BIOS drive
 * OUT: CF clear on success. AX/BX preserved.
 */
read_lba_sector:
    push ax
    push bx
    push cx
    push dx
    push di

    mov di, bx
    xor dx, dx
    mov cx, FLOPPY_SECTORS_PER_TRACK
    div cx
    mov cl, dl
    inc cl
    xor dx, dx
    mov bx, FLOPPY_HEADS
    div bx
    mov ch, al
    mov dh, dl
    mov dl, [boot_drive]
    mov bx, di
    mov ah, 0x02
    mov al, 0x01
    int 0x13

    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

.section .data
boot_drive:
    .byte 0x00
kernel_lba:
    .word 0x0000
kernel_sectors:
    .word 0x0000
