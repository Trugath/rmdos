/*
 * INT 2Fh — multiplex. Install-check probes return "not installed"
 * cleanly; DOS internal AH=12 AL=0 answers installed.
 */

install_int2f_vector:
    push ax
    push bx
    push es
    xor ax, ax
    mov es, ax
    lea bx, [int2f_handler]
    mov word ptr es:[0x2F * 4], bx
    mov word ptr es:[0x2F * 4 + 2], cs
    pop es
    pop bx
    pop ax
    ret

int2f_handler:
    sti
    cmp ah, 0x12
    je .i2f_12
    cmp ah, 0x01
    je .i2f_not_inst
    cmp ah, 0x10
    je .i2f_not_inst
    cmp ah, 0x43
    je .i2f_not_inst
    cmp ax, 0x1600
    je .i2f_1600
    cmp ah, 0x16
    je .i2f_iret
    cmp ax, 0x1680
    je .i2f_iret
    cmp ah, 0xB7
    je .i2f_append
    iret

.i2f_12:
    /* DOS internal multiplex — AL=0 → installed */
    cmp al, 0
    jne .i2f_iret
    mov al, 0xFF
    iret

.i2f_1600:
    /* Windows enhanced mode check — return AL=0 (not present) */
    xor al, al
    iret

.i2f_append:
    /* AH=B7 AL=0 APPEND install check → not installed */
    cmp al, 0
    jne .i2f_iret
    /* fall through */
.i2f_not_inst:
    xor al, al
    iret

.i2f_iret:
    iret
