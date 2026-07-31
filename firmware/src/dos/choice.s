.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * CHOICE.COM — prompt for a key among choices.
 * Supports: /C[:]choices  /N  /T[:]default,seconds
 * Exit ERRORLEVEL = 1-based index of choice (CHOICE-compatible).
 */

_start:
    push cs
    pop ds
    push cs
    pop es

    /* defaults: YN, show prompt, no timeout */
    lea si, [def_choices]
    lea di, [choices]
    mov cx, 3
    rep movsb
    mov byte ptr [no_prompt], 0
    mov byte ptr [have_timeout], 0
    mov byte ptr [def_choice], 'Y'
    mov word ptr [timeout_sec], 0

    mov si, 0x81
.parse:
    call skip_spaces
    jc .parsed
    cmp byte ptr [si], '/'
    je .switch
    cmp byte ptr [si], '-'
    je .switch
    /* trailing prompt text — skip rest */
    jmp .parsed

.switch:
    inc si
    lodsb
    call toupper_al
    cmp al, 'C'
    je .sw_c
    cmp al, 'N'
    je .sw_n
    cmp al, 'T'
    je .sw_t
    jmp .parse

.sw_n:
    mov byte ptr [no_prompt], 1
    jmp .parse

.sw_c:
    call eat_colon
    lea di, [choices]
.copy_c:
    lodsb
    cmp al, ' '
    je .c_done
    cmp al, 0x09
    je .c_done
    cmp al, '/'
    je .c_slash
    cmp al, '-'
    je .c_slash
    cmp al, 0x0D
    je .c_done
    cmp al, 0
    je .c_done
    call toupper_al
    stosb
    jmp .copy_c
.c_slash:
    dec si
.c_done:
    mov byte ptr [di], 0
    lea ax, [choices]
    cmp di, ax
    jne .parse
    /* empty /C — restore default */
    lea si, [def_choices]
    lea di, [choices]
    mov cx, 3
    rep movsb
    jmp .parse

.sw_t:
    call eat_colon
    lodsb
    cmp al, 0x0D
    je .parse
    cmp al, 0
    je .parse
    call toupper_al
    mov [def_choice], al
    lodsb
    cmp al, ','
    jne .parse
    xor bx, bx
.t_digits:
    lodsb
    cmp al, '0'
    jb .t_end
    cmp al, '9'
    ja .t_end
    sub al, '0'
    mov ah, 0
    /* bx = bx*10 + al */
    push ax
    mov ax, bx
    mov cx, 10
    mul cx
    mov bx, ax
    pop ax
    add bx, ax
    jmp .t_digits
.t_end:
    dec si
    mov [timeout_sec], bx
    mov byte ptr [have_timeout], 1
    jmp .parse

.parsed:
    cmp byte ptr [no_prompt], 0
    jne .wait
    /* print [A,B,?]? */
    mov dl, '['
    mov ah, 0x02
    int 0x21
    lea si, [choices]
    mov byte ptr [first_ch], 1
.pr_loop:
    lodsb
    test al, al
    jz .pr_end
    cmp byte ptr [first_ch], 0
    jne .pr_no_comma
    mov dl, ','
    push ax
    mov ah, 0x02
    int 0x21
    pop ax
.pr_no_comma:
    mov byte ptr [first_ch], 0
    mov dl, al
    mov ah, 0x02
    int 0x21
    jmp .pr_loop
.pr_end:
    mov ah, 0x09
    lea dx, [msg_prompt_end]
    int 0x21

.wait:
    /* start tick for timeout */
    cmp byte ptr [have_timeout], 0
    je .wait_loop
    xor ah, ah
    int 0x1A
    mov [start_tick], dx
    mov ax, [timeout_sec]
    mov cx, 18
    mul cx
    mov [tick_limit], ax

.wait_loop:
    mov ah, 0x0B
    int 0x21
    test al, al
    jz .check_to
    mov ah, 0x08
    int 0x21
    call toupper_al
    mov bl, al
    call find_choice
    jc .wait_loop
    /* AL = 1-based index */
    mov ah, 0x4C
    int 0x21

.check_to:
    cmp byte ptr [have_timeout], 0
    je .wait_loop
    xor ah, ah
    int 0x1A
    sub dx, [start_tick]
    cmp dx, [tick_limit]
    jb .wait_loop
    /* timed out — use default */
    mov bl, [def_choice]
    call find_choice
    jc .to_first
    mov ah, 0x4C
    int 0x21
.to_first:
    mov al, 1
    mov ah, 0x4C
    int 0x21

/* BL = uppercased key → AL = index (1..), CF=miss */
find_choice:
    lea si, [choices]
    mov cl, 1
.fc_loop:
    lodsb
    test al, al
    jz .fc_miss
    cmp al, bl
    je .fc_hit
    inc cl
    jmp .fc_loop
.fc_hit:
    mov al, cl
    clc
    ret
.fc_miss:
    stc
    ret

skip_spaces:
    lodsb
    cmp al, ' '
    je skip_spaces
    cmp al, 0x09
    je skip_spaces
    cmp al, 0x0D
    je .ss_empty
    cmp al, 0
    je .ss_empty
    dec si
    clc
    ret
.ss_empty:
    stc
    ret

eat_colon:
    cmp byte ptr [si], ':'
    jne .ec_done
    inc si
.ec_done:
    ret

toupper_al:
    cmp al, 'a'
    jb .tu_done
    cmp al, 'z'
    ja .tu_done
    sub al, 0x20
.tu_done:
    ret

def_choices:
    .asciz "YN"
choices:
    .space 32, 0
msg_prompt_end:
    .ascii "]? $"
no_prompt:
    .byte 0
have_timeout:
    .byte 0
def_choice:
    .byte 'Y'
timeout_sec:
    .word 0
start_tick:
    .word 0
tick_limit:
    .word 0
first_ch:
    .byte 0
