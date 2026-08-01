.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * rmDOS bootstrap (FAT12 or FAT16 volumes).
 *
 * BIOS loads us at 0000:7C00. We relocate to 0000:0600, then load KERNEL at
 * 0070:0000 (phys 0x0700), which clobbers 0x0700-0x07FF. All code used after
 * relocate therefore lives in the first 256 bytes (phys 0x0600-0x06FF). The
 * cold entry (relocate stub) sits above 0x100 and is only run from 7C00.
 * CHS uses BPB SPT/heads at DS:0x18 / DS:0x1A with DS=0060 after relocate.
 * Drive number is kept in BPB BS_DrvNum (offset 0x24).
 * Kernel load uses the RFAT1 reserved sector (absolute LBA), not a FAT walk.
 */

.equ KERNEL_SEGMENT, 0x0070
.equ BOOT_ORIGIN, 0x7C00
.equ BOOT_RELOC_SEG, 0x0060

_start:
    /* Fixed three-byte jump keeps the BPB at its canonical offsets. */
    .byte 0xE9
    .word boot_start - . - 2
    .space 59, 0

/* ---- hot path: must remain below offset 0x100 ---- */

/* AX=LBA, ES:BX=buf; DS=0060. Preserves AX/BX. */
read_lba:
    push ax
    push bx
    push cx
    push dx
    push si

    mov si, bx
    add ax, [0x1C]               /* BPB HiddenSectors (partition base) */
    xor dx, dx
    mov cx, [0x18]
    div cx
    mov cl, dl
    inc cl
    xor dx, dx
    mov bx, [0x1A]
    div bx
    mov dh, dl
    mov ch, al
    mov al, ah
    mov ah, cl
    mov cl, 6
    shl al, cl
    or al, ah
    mov cl, al
    mov dl, [0x24]
    mov bx, si
    mov ax, 0x0201
    int 0x13

    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

after_reloc:
    mov ax, BOOT_RELOC_SEG
    mov ds, ax
    xor ax, ax
    mov ss, ax
    mov sp, 0x0600

    mov es, ax
    mov bx, BOOT_ORIGIN
    mov ax, 1
    call read_lba
    jc hang

    cmp word ptr es:[BOOT_ORIGIN], 0x4652
    jne hang
    cmp word ptr es:[BOOT_ORIGIN + 2], 0x5441
    jne hang
    cmp byte ptr es:[BOOT_ORIGIN + 4], '1'
    jne hang

    mov si, es:[BOOT_ORIGIN + 0x1C]   /* kernel LBA */
    mov cx, es:[BOOT_ORIGIN + 0x1E]   /* sector count */

    mov ax, KERNEL_SEGMENT
    mov es, ax
    xor bx, bx

.load_loop:
    jcxz .go_kernel
    mov ax, si
    call read_lba
    jc hang
    inc si
    add bx, 512
    dec cx
    jmp .load_loop

.go_kernel:
    mov dl, [0x24]
    sti
    jmp KERNEL_SEGMENT, 0x0000

hang:
    hlt
    jmp hang

/* ---- cold path: runs only at 7C00 before relocate ---- */

boot_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, BOOT_ORIGIN
    mov [BOOT_ORIGIN + 0x24], dl

    mov si, BOOT_ORIGIN
    mov di, 0x0600
    mov cx, 256
    cld
    rep movsw
    jmp BOOT_RELOC_SEG, (after_reloc - _start)
