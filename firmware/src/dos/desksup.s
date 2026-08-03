.code16
.intel_syntax noprefix

/*
 * DESKSUP.COM — tiny desk process supervisor for rmDesk.
 *
 * Not an AH=31h TSR: stays as AH=4Bh parent so foreign apps never share
 * DESK.EXE's resident image.
 *
 * Loop:
 *   1. EXEC A:\DESK\DESK.EXE
 *   2. If A:\DESK\PENDING exists: read path, delete, EXEC path, goto 1
 *   3. Else DESK quit with no pending → AH=4Ch (back to COMMAND)
 */

.section .text
.global _start
_start:
    push cs
    pop ds
    push cs
    pop es

.loop_desk:
    lea dx, [path_desk]
    call do_exec
    /* CF set → desk failed to start; give up */
    jc .exit

    /* Try open PENDING (read-only) */
    lea dx, [path_pending]
    mov ax, 0x3D00
    int 0x21
    jc .exit                   /* no pending → desk quit intentionally */
    mov bx, ax                 /* BX = handle */

    /* Read path into path_buf (leave room for NUL) */
    lea dx, [path_buf]
    mov cx, 120
    mov ah, 0x3F
    int 0x21
    jc .close_exit
    mov cx, ax                 /* bytes read */
    mov si, offset path_buf
    add si, cx
    /* Strip trailing CR/LF and NUL-terminate */
.trim:
    cmp cx, 0
    je .trimmed
    dec si
    dec cx
    mov al, [si]
    cmp al, 13
    je .trim
    cmp al, 10
    je .trim
    cmp al, 0
    je .trim
    inc si
    inc cx
.trimmed:
    mov byte ptr [si], 0

    mov ah, 0x3E
    int 0x21                   /* close PENDING */

    /* Delete PENDING before EXEC so a crash does not re-launch forever */
    lea dx, [path_pending]
    mov ah, 0x41
    int 0x21

    cmp byte ptr [path_buf], 0
    je .loop_desk              /* empty path → relaunch desk */

    lea dx, [path_buf]
    call do_exec
    /* Child returned (or failed) → always go back to desk */
    jmp .loop_desk

.close_exit:
    mov ah, 0x3E
    int 0x21
.exit:
    mov ax, 0x4C00
    int 0x21

/*
 * IN:  DS:DX = ASCIZ program path
 * OUT: CF clear on success (child ran and returned), CF set on EXEC failure
 * Clobbers AX,BX; restores DS=CS
 */
do_exec:
    push dx
    call make_epb
    pop dx
    push es
    push ds
    pop es
    mov bx, offset exec_pb
    mov ax, 0x4B00
    int 0x21
    pop es
    push cs
    pop ds
    ret

make_epb:
    /* Empty command tail: length 0, CR */
    mov byte ptr [exec_tail], 0
    mov byte ptr [exec_tail + 1], 13
    /* Clear FCBs */
    push di
    push cx
    lea di, [fcb1]
    mov cx, 16
    xor ax, ax
    rep stosb
    lea di, [fcb2]
    mov cx, 16
    rep stosb
    pop cx
    pop di
    /* EPB: inherit env from our PSP:002C */
    mov ax, [0x2C]
    mov word ptr [exec_pb], ax
    mov word ptr [exec_pb + 2], offset exec_tail
    mov ax, ds
    mov word ptr [exec_pb + 4], ax
    mov word ptr [exec_pb + 6], offset fcb1
    mov word ptr [exec_pb + 8], ax
    mov word ptr [exec_pb + 10], offset fcb2
    mov word ptr [exec_pb + 12], ax
    ret

path_desk:
    .asciz "A:\\DESK\\DESK.EXE"
path_pending:
    .asciz "A:\\DESK\\PENDING"

exec_pb:
    .space 14, 0
exec_tail:
    .space 2, 0
fcb1:
    .space 16, 0
fcb2:
    .space 16, 0
path_buf:
    .space 128, 0
