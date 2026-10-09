.code16
.intel_syntax noprefix

/*
 * FE2GO.COM — lean Frontier Elite II shell for os-fe2.img.
 *
 * COMMAND.COM is ~58 KiB resident while a child runs. This tiny COM is SHELL=,
 * shrinks itself, switches to C:, then EXEC C:\FRONTIER.EXE.
 */

.section .text
.global _start

.equ OFF_EPB,   0
.equ OFF_TAIL,  14
.equ OFF_FCB,   16
.equ SCRATCH,   48

_start:
    push cs
    pop ds
    push cs
    pop es

    /* Shrink to code + scratch + ~512 bytes stack (paragraphs from PSP). */
    lea ax, [img_end + SCRATCH + 512]
    add ax, 15
    mov cl, 4
    shr ax, cl
    mov bx, ax
    mov ah, 0x4A
    int 0x21

    /* Default drive C: (2) so Frontier finds overlays next to the EXE. */
    mov ah, 0x0E
    mov dl, 2
    int 0x21
    mov ah, 0x3B
    lea dx, [path_root]
    int 0x21

    call init_epb
    lea dx, [path_fe2]
    call do_exec

    mov ax, 0x4C00
    int 0x21

init_epb:
    lea di, [img_end]
    xor ax, ax
    stosw
    lea ax, [img_end + OFF_TAIL]
    stosw
    mov ax, ds
    stosw
    lea ax, [img_end + OFF_FCB]
    stosw
    mov ax, ds
    stosw
    lea ax, [img_end + OFF_FCB]
    stosw
    mov ax, ds
    stosw
    xor ax, ax
    stosb
    mov al, 13
    stosb
    xor ax, ax
    mov cx, 8
    rep stosw
    ret

do_exec:
    push dx
    mov ax, [0x2C]
    mov [img_end + OFF_EPB], ax
    pop dx
    lea bx, [img_end + OFF_EPB]
    mov ax, 0x4B00
    int 0x21
    push cs
    pop ds
    ret

path_root:
    .asciz "C:\\"
path_fe2:
    .asciz "C:\\FRONTIER.EXE"

img_end:
