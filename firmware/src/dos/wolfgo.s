.code16
.intel_syntax noprefix

/*
 * WOLFGO.COM — lean Wolf3D shell for os-wolf3d.img.
 *
 * COMMAND.COM is ~58 KiB resident while a child runs; that alone keeps the
 * shareware MAIN gauge under the 256 KiB bar. This tiny COM is SHELL=,
 * shrinks itself, switches to C:, then EXEC C:\WOLF3D.EXE.
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

    /* Default drive C: (2) so Wolf finds .WL1 next to the EXE. */
    mov ah, 0x0E
    mov dl, 2
    int 0x21
    mov ah, 0x3B
    lea dx, [path_root]
    int 0x21

    call init_epb
    lea dx, [path_wolf]
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
path_wolf:
    .asciz "C:\\WOLF3D.EXE"

img_end:
