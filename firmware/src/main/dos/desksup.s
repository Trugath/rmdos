.code16
.intel_syntax noprefix

/*
 * DESKSUP.COM — tiny desk process supervisor for rmDesk.
 *
 * Loop:
 *   1. EXEC DESK\DESK.EXE
 *   2. If DESK\PENDING exists: read app (+ optional cwd line), delete,
 *      AH=3Bh cwd if present, EXEC app, AH=3Bh A:\, goto 1
 *   3. Else quit → AH=4Ch
 *
 * PENDING: line1=app path, line2=folder cwd (optional). Both stay in the
 * OFF_PATH scratch buffer (cwd is the bytes after the first NUL).
 */

.section .text
.global _start

.equ OFF_EPB,     0
.equ OFF_TAIL,    14
.equ OFF_FCB,     16
.equ OFF_PATH,    32
.equ OFF_CWDP,    192
.equ READMAX,     160

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
    mov cx, READMAX
    mov ah, 0x3F
    int 0x21
    jc .close_exit
    mov cx, ax
    call parse_pending
    mov ah, 0x3E
    int 0x21
    mov dx, offset path_pend
    mov ah, 0x41
    int 0x21

    cmp byte ptr [img_end + OFF_PATH], 0
    je .loop_desk

    mov bx, [img_end + OFF_CWDP]
    cmp byte ptr [bx], 0
    je .do_child
    mov dx, bx
    mov ah, 0x3B
    int 0x21
.do_child:
    lea dx, [img_end + OFF_PATH]
    call do_exec
    mov dx, offset path_root
    mov ah, 0x3B
    int 0x21
    jmp .loop_desk

.close_exit:
    mov ah, 0x3E
    int 0x21
.exit:
    mov ax, 0x4C00
    int 0x21

/* CX=bytes read. NUL-split line1/line2 in OFF_PATH; OFF_CWDP = cwd ptr. */
parse_pending:
    lea ax, [img_end + OFF_PATH]
    mov [img_end + OFF_CWDP], ax
    or cx, cx
    jnz .pp_go
    mov byte ptr [img_end + OFF_PATH], 0
    ret
.pp_go:
    lea si, [img_end + OFF_PATH]
    add si, cx
.pp_trim:
    lea bx, [img_end + OFF_PATH]
    cmp si, bx
    ja .pp_t1
    mov byte ptr [img_end + OFF_PATH], 0
    ret
.pp_t1:
    dec si
    mov al, [si]
    cmp al, 13
    je .pp_trim
    cmp al, 10
    je .pp_trim
    cmp al, 0
    je .pp_trim
    inc si
    mov byte ptr [si], 0
    lea si, [img_end + OFF_PATH]
.pp_find:
    mov al, [si]
    or al, al
    jz .pp_done
    cmp al, 13
    je .pp_split
    cmp al, 10
    je .pp_split
    inc si
    jmp .pp_find
.pp_split:
    mov byte ptr [si], 0
    inc si
.pp_skip:
    mov al, [si]
    cmp al, 13
    je .pp_sk
    cmp al, 10
    je .pp_sk
    mov [img_end + OFF_CWDP], si
    jmp .pp_done
.pp_sk:
    inc si
    jmp .pp_skip
.pp_done:
    ret

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

path_desk:
    .asciz "DESK\\DESK.EXE"
path_pend:
    .asciz "DESK\\PENDING"
path_root:
    .asciz "A:\\"

img_end:
