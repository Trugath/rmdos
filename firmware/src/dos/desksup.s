.code16
.intel_syntax noprefix

/*
 * DESKSUP.COM — tiny desk process supervisor for rmDesk.
 *
 * Not an AH=31h TSR: stays as AH=4Bh parent so foreign apps never share
 * DESK.EXE's resident image.
 *
 * Loop:
 *   1. EXEC DESK\DESK.EXE
 *   2. If DESK\PENDING exists: read path, delete, EXEC path, goto 1
 *   3. Else DESK quit with no pending → AH=4Ch (back to COMMAND)
 *
 * Relative paths assume CWD A:\ (AUTOEXEC). Scratch (EPB/FCB/path) lives in
 * RAM past the COM image so the on-disk COM stays code+strings only.
 */

.section .text
.global _start

/* Scratch layout at img_end (not stored in the .COM file). */
.equ OFF_EPB,     0
.equ OFF_TAIL,    14
.equ OFF_FCB,     16
.equ OFF_PATH,    32
.equ PATHMAX,     80

_start:
    push cs
    pop ds
    push cs
    pop es
    call init_epb

.loop_desk:
    mov dx, offset path_desk
    call do_exec
    jc .exit

    mov dx, offset path_pend
    mov ax, 0x3D00
    int 0x21
    jc .exit
    xchg bx, ax

    lea dx, [img_end + OFF_PATH]
    mov cx, PATHMAX
    mov ah, 0x3F
    int 0x21
    jc .close_exit
    lea si, [img_end + OFF_PATH]
    add si, ax
    lea bx, [img_end + OFF_PATH]
.trim:
    cmp si, bx
    je .nul
    dec si
    mov al, [si]
    cmp al, 13
    je .trim
    cmp al, 10
    je .trim
    cmp al, 0
    je .trim
    inc si
.nul:
    mov byte ptr [si], 0

    mov ah, 0x3E
    int 0x21
    mov dx, offset path_pend
    mov ah, 0x41
    int 0x21

    cmp byte ptr [img_end + OFF_PATH], 0
    je .loop_desk
    lea dx, [img_end + OFF_PATH]
    call do_exec
    jmp .loop_desk

.close_exit:
    mov ah, 0x3E
    int 0x21
.exit:
    mov ax, 0x4C00
    int 0x21

init_epb:
    lea di, [img_end]
    /* EPB: env, tail off/seg, fcb1 off/seg, fcb2 off/seg */
    xor ax, ax
    stosw                       /* env (filled in do_exec) */
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
    /* empty tail: len=0, CR */
    xor ax, ax
    stosb
    mov al, 13
    stosb
    /* zero shared FCB (16 bytes); DI already at OFF_FCB */
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

path_desk:
    .asciz "DESK\\DESK.EXE"
path_pend:
    .asciz "DESK\\PENDING"

img_end:
