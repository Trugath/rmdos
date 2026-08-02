/*
 * DOS MCB memory arena (AH=48/49/4A).
 *
 * MCB at segment S: +0 'M'/'Z', +1 owner (0=free), +3 size in paragraphs
 * (excluding MCB). Allocated user segment = S+1.
 */

mem_init:
    push ax
    push bx
    push cx
    push dx
    push es

    int 0x12
    mov cl, 6
    shl ax, cl
    mov word ptr cs:[mem_top], ax

    lea ax, [kernel_end]
    add ax, 15
    mov cl, 4
    shr ax, cl
    mov dx, cs
    add dx, ax
    mov word ptr cs:[first_mcb], dx

    mov es, dx
    mov byte ptr es:[0], 'Z'
    mov word ptr es:[1], 0
    mov ax, word ptr cs:[mem_top]
    sub ax, dx
    dec ax
    mov word ptr es:[3], ax

    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret

/* AH=48 BX=paras → AX=seg. Strategy in dos_alloc_strategy low bits:
 * 0=first fit, 1=best fit, 2=last fit. */
dos_ah48:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    push bp

    mov dx, bx                   /* DX = request size */
    mov al, byte ptr cs:[dos_alloc_strategy]
    and al, 3
    mov ah, al                   /* AH = strategy */

    xor si, si                   /* SI = best/last candidate MCB seg (0=none) */
    xor bp, bp                   /* BP = best size (best-fit) */
    mov ax, word ptr cs:[first_mcb]
.a48_scan:
    mov es, ax
    mov bl, es:[0]
    cmp bl, 'M'
    je .a48_try
    cmp bl, 'Z'
    je .a48_try
    jmp .a48_fail
.a48_try:
    cmp word ptr es:[1], 0
    jne .a48_adv
    cmp word ptr es:[3], dx
    jb .a48_adv
    /* free block large enough */
    cmp ah, 0
    je .a48_first
    cmp ah, 1
    je .a48_best
    /* last fit: keep highest address */
    mov si, ax
    jmp .a48_adv
.a48_best:
    test si, si
    jz .a48_best_take
    mov bx, es:[3]
    cmp bx, bp
    jae .a48_adv
.a48_best_take:
    mov si, ax
    mov bp, es:[3]
    jmp .a48_adv
.a48_first:
    mov si, ax
    jmp .a48_found

.a48_adv:
    cmp byte ptr es:[0], 'Z'
    je .a48_scan_done
    mov bx, es:[3]
    inc bx
    add ax, bx
    jmp .a48_scan

.a48_scan_done:
    test si, si
    jnz .a48_found
    jmp .a48_fail

.a48_found:
    /* take free block at SI, size DX */
    mov es, si
    mov cx, es:[3]
    cmp cx, dx
    je .a48_take
    mov bl, byte ptr es:[0]
    mov di, si
    inc di
    add di, dx
    mov es, di
    mov byte ptr es:[0], bl
    mov word ptr es:[1], 0
    mov bx, cx
    sub bx, dx
    dec bx
    mov word ptr es:[3], bx
    mov es, si
    mov byte ptr es:[0], 'M'
.a48_take:
    mov es, si
    mov word ptr es:[3], dx
    mov word ptr es:[1], 0x00FF
    mov ax, si
    inc ax
    pop bp
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    clc
    jmp int21_iret

.a48_fail:
    mov ax, word ptr cs:[first_mcb]
    xor cx, cx
.a48_lg:
    mov es, ax
    cmp word ptr es:[1], 0
    jne .a48_ln
    mov bx, es:[3]
    cmp bx, cx
    jbe .a48_ln
    mov cx, bx
.a48_ln:
    cmp byte ptr es:[0], 'Z'
    je .a48_lg_done
    mov bx, es:[3]
    inc bx
    add ax, bx
    jmp .a48_lg
.a48_lg_done:
    pop bp
    pop es
    pop di
    pop si
    pop dx
    add sp, 4                    /* drop saved cx, bx */
    mov bx, cx                   /* largest free */
    mov ax, 8
    stc
    jmp int21_iret_cf

dos_ah49:
    push ax
    push bx
    push cx
    push dx
    push es
    mov ax, es
    dec ax
    mov es, ax
    mov al, es:[0]
    cmp al, 'M'
    je .a49_ok
    cmp al, 'Z'
    jne .a49_err
.a49_ok:
    mov word ptr es:[1], 0
    call mem_coalesce
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    clc
    jmp int21_iret
.a49_err:
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    mov ax, 9
    stc
    jmp int21_iret_cf

dos_ah4a:
    push ax
    push cx
    push dx
    push si
    push es
    mov ax, es
    dec ax
    mov es, ax
    mov cx, es:[3]
    cmp bx, cx
    ja .a4a_grow
    je .a4a_ok
    /* Split: free tail inherits original 'M'/'Z' (do not force Z). */
    mov dx, bx
    mov bl, byte ptr es:[0]
    mov si, es
    mov ax, si
    inc ax
    add ax, dx
    push es
    mov es, ax
    mov byte ptr es:[0], bl
    mov word ptr es:[1], 0
    mov bx, cx
    sub bx, dx
    dec bx
    mov word ptr es:[3], bx
    pop es
    mov byte ptr es:[0], 'M'
    mov word ptr es:[3], dx
    call mem_coalesce
.a4a_ok:
    pop es
    pop si
    pop dx
    pop cx
    pop ax
    clc
    jmp int21_iret

.a4a_grow:
    /* Grow only if this is the last MCB and mem_top has room */
    cmp byte ptr es:[0], 'Z'
    jne .a4a_fail
    mov ax, es
    inc ax
    add ax, bx
    cmp ax, word ptr cs:[mem_top]
    ja .a4a_fail
    mov word ptr es:[3], bx
    jmp .a4a_ok

.a4a_fail:
    mov bx, cx
    pop es
    pop si
    pop dx
    pop cx
    pop ax
    mov ax, 8
    stc
    jmp int21_iret_cf

mem_coalesce:
    push ax
    push bx
    push cx
    push dx
    push es
.mc_again:
    mov ax, word ptr cs:[first_mcb]
.mc_scan:
    mov es, ax
    cmp word ptr es:[1], 0
    jne .mc_next
    cmp byte ptr es:[0], 'Z'
    je .mc_done
    mov bx, es:[3]
    inc bx
    add bx, ax
    push es
    mov es, bx
    cmp word ptr es:[1], 0
    jne .mc_nn
    mov cl, es:[0]
    mov dx, es:[3]
    pop es
    inc dx
    add word ptr es:[3], dx
    mov es:[0], cl
    jmp .mc_again
.mc_nn:
    pop es
.mc_next:
    cmp byte ptr es:[0], 'Z'
    je .mc_done
    mov bx, es:[3]
    inc bx
    add ax, bx
    jmp .mc_scan
.mc_done:
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret
