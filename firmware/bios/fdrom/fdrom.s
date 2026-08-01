/*
 * rmDOS Fixed Disk option ROM (C800:0000), 2 KiB.
 * INT 13h DL>=80h via Wd1003 @ 0x320 / DMA3 / IRQ5.
 * Host patches .geo at offset 0x700: two×{cyl_lo,cyl_hi,heads,spt}.
 */
.code16
.intel_syntax noprefix

.equ HDC_DATA, 0x320
.equ HDC_STAT, 0x321
.equ HDC_SEL,  0x322
.equ HDC_CTL,  0x323
.equ PIC_CMD,  0x20
.equ PIC_IMR,  0x21
.equ DMA_A3,   0x06
.equ DMA_C3,   0x07
.equ DMA_FF,   0x0C
.equ DMA_MODE, 0x0B
.equ DMA_MASK, 0x0A
.equ DMA_PG3,  0x82
.equ BDA,      0x40
.equ BDA_ST,   0x74
.equ BDA_CNT,  0x75

.section .text
.global _start
_start:
    .byte 0x55, 0xAA, 0x04
    jmp init

init:
    push ds
    push es
    push ax
    push dx
    push cs
    pop ds
    xor ax, ax
    mov es, ax
    cli
    /* Hook IRQ5 (INT 0Dh) before unmasking */
    mov word ptr es:[0x34], offset i0d
    mov word ptr es:[0x36], cs
    mov ax, es:[0x4C]
    mov [o13], ax
    mov ax, es:[0x4E]
    mov [o13+2], ax
    mov word ptr es:[0x4C], offset i13
    mov word ptr es:[0x4E], cs
    in al, PIC_IMR
    and al, 0xDF
    out PIC_IMR, al
    sti
    mov ax, BDA
    mov es, ax
    xor al, al
    cmp word ptr [geo], 0
    je 1f
    inc al
1:  cmp word ptr [geo+4], 0
    je 2f
    inc al
2:  mov es:[BDA_CNT], al
    /* Select controller + enable IRQ (do not write status/reset port) */
    mov dx, HDC_SEL
    out dx, al
    mov dx, HDC_CTL
    mov al, 3
    out dx, al
    pop dx
    pop ax
    pop es
    pop ds
    retf

i0d:
    push ax
    push ds
    push cs
    pop ds
    mov byte ptr [irqf], 1
    mov al, 0x20
    out PIC_CMD, al
    pop ds
    pop ax
    iret

i13:
    cmp dl, 0x80
    jae do_hd
    jmp dword ptr cs:[o13]
do_hd:
    cli
    push ds
    push es
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push cs
    pop ds
    mov [sa], ax
    mov [sb], bx
    mov [sc], cx
    mov [sd], dx
    mov [se], es

    cmp ah, 0
    je zrst
    cmp ah, 0x0D
    je zrst
    cmp ah, 1
    je zst
    cmp ah, 2
    je zrd
    cmp ah, 3
    je zwr
    cmp ah, 4
    je zvf
    cmp ah, 5
    je zfm
    cmp ah, 8
    je zpr
    cmp ah, 9
    je zok
    cmp ah, 0x0C
    je zok
    cmp ah, 0x15
    je zds
    mov ah, 1
    jmp bad

zrst:
    call rst
zok:
    xor ah, ah
    jmp good
zst:
    mov ah, [lst]
    test ah, ah
    jnz bad
    jmp good
zrd:
    mov byte ptr [md], 0
    call xfer
    jmp fin
zwr:
    mov byte ptr [md], 1
    call xfer
    jmp fin
zvf:
    mov byte ptr [md], 2
    call xfer
    jmp fin
zfm:
    call fmt
    jmp fin
zpr:
    call prm
    jmp fin
zds:
    call dsd
    jmp fin

good:
    xor ah, ah
fin:
    mov [lst], ah
    push ax
    mov ax, BDA
    mov es, ax
    pop ax
    mov es:[BDA_ST], ah
    mov [sa+1], ah
    test ah, ah
    jnz bad2
    mov [sa], al
    mov byte ptr [cfl], 0
    jmp out
bad:
    mov [lst], ah
bad2:
    mov byte ptr [sa], 0
    mov [sa+1], ah
    mov byte ptr [cfl], 1
out:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    mov ax, [sa]
    cmp byte ptr [sa+1], 0
    jne skip_cxdx
    mov cx, [sc]
    mov dx, [sd]
skip_cxdx:
    pop es
    pop ds
    push bp
    mov bp, sp
    cmp byte ptr cs:[cfl], 0
    jne set_cf
    and word ptr [bp+6], 0xFFFE
    jmp iret_hd
set_cf:
    or word ptr [bp+6], 1
iret_hd:
    pop bp
    sti
    iret

rst:
    push dx
    mov dx, HDC_STAT
    out dx, al
    mov dx, HDC_CTL
    mov al, 3
    out dx, al
    pop dx
    ret

xfer:
    sti
    mov al, [sa]
    test al, al
    jnz 1f
    xor ah, ah
    ret
1:  mov [n], al
    mov al, [sd]
    and al, 1
    mov [dr], al
    mov al, [sd+1]
    and al, 0x1F
    mov [hd], al
    mov ax, [sc]                    /* AL=CL AH=CH */
    mov bl, al
    and bl, 0x3F
    mov [sec], bl
    mov bh, al
    mov cl, 6
    shr bh, cl
    and bh, 3
    mov al, ah                      /* CH */
    mov ah, bh                      /* cyl[9:8] */
    mov [cy], ax
    mov es, [se]
    mov bx, [sb]
    mov al, [n]
    xor ah, ah
    mov cl, 9
    shl ax, cl
    mov cx, ax
    cmp byte ptr [md], 2
    jne 2f
    push cs
    pop es
    mov bx, offset vb
    mov cx, 512
2:  mov al, [md]
    call dma
    jc xb
    mov byte ptr [irqf], 0
    mov dx, HDC_SEL
    out dx, al
    cmp byte ptr [md], 1
    je 3f
    mov al, 0x08
    jmp 4f
3:  mov al, 0x0A
4:  call cmd6
    call wait
    jc xt
    call rcsb
    jc xe
    mov ah, 0
    mov al, [n]
    ret
xb: mov ah, 9
    ret
xt: mov ah, 0x80
    ret
xe: mov ah, 0x20
    ret

fmt:
    sti
    mov al, [sd]
    and al, 1
    mov [dr], al
    mov al, [sd+1]
    and al, 0x1F
    mov [hd], al
    mov al, [sc+1]
    xor ah, ah
    mov [cy], ax
    mov byte ptr [sec], 1
    mov byte ptr [n], 1
    mov byte ptr [irqf], 0
    mov dx, HDC_SEL
    out dx, al
    mov al, 6
    call cmd6
    call wait
    jc 1f
    call rcsb
    jc 1f
    xor ah, ah
    ret
1:  mov ah, 0x20
    ret

prm:
    push cs
    pop ds
    /* Drive 0 geometry (DL bit0 selects drive 1 table at geo+4) */
    xor bx, bx
    test byte ptr [sd], 1
    jz prm_d0
    mov bx, 4
prm_d0:
    mov ax, cs:[bx + geo]
    test ax, ax
    jnz prm_have
    /* Fall back to XT 306-cyl if table empty */
    mov bx, 0
    mov ax, cs:[geo]
    test ax, ax
    jnz prm_have
    mov ax, 306
prm_have:
    dec ax
    mov [sc+1], al
    mov ah, cs:[bx + geo + 1]
    and ah, 3
    mov cl, 6
    shl ah, cl
    mov al, cs:[bx + geo + 3]
    and al, 0x3F
    or al, ah
    mov [sc], al
    mov al, cs:[bx + geo + 2]
    test al, al
    jnz prm_hd
    mov al, 4
prm_hd:
    dec al
    mov [sd+1], al
    mov al, 1
    cmp word ptr cs:[geo + 4], 0
    je prm_cnt
    mov al, 2
prm_cnt:
    mov [sd], al
    xor ah, ah
    ret

dsd:
    mov bl, [sd]
    and bx, 1
    shl bx, 2
    mov ax, [geo+bx]
    test ax, ax
    jnz 1f
    xor ah, ah
    stc
    ret
1:  mov cl, [geo+bx+2]
    mov ch, [geo+bx+3]
    mul cl
    mov cl, ch
    xor ch, ch
    mul cx
    mov [sc], dx
    mov [sd], ax
    mov ah, 2
    clc
    ret

/* AL 0=read(dev→mem) 1=write(mem→dev); ES:BX CX=len */
dma:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov si, ax                      /* direction */
    /* phys = ES<<4+BX, page = ES>>12 */
    mov ax, es
    mov dx, ax
    mov cl, 4
    shl ax, cl
    mov di, ax
    mov ax, dx
    mov cl, 12
    shr ax, cl
    add di, bx
    adc al, 0
    mov ah, al                      /* AH=page, DI=off */
    mov bx, sp
    mov cx, ss:[bx+6]               /* saved CX */
    mov dx, di
    add dx, cx
    jc dbad
    dec cx
    push ax                         /* save page in AH */
    mov al, 7
    mov dx, DMA_MASK
    out dx, al
    mov dx, DMA_FF
    out dx, al
    mov dx, DMA_A3
    mov ax, di
    out dx, al
    mov al, ah
    out dx, al
    mov dx, DMA_FF
    out dx, al
    mov dx, DMA_C3
    mov ax, cx
    out dx, al
    mov al, ah
    out dx, al
    pop ax
    mov dx, DMA_PG3
    mov al, ah                      /* page */
    out dx, al
    test si, 1
    jnz 1f
    mov al, 0x47
    jmp 2f
1:  mov al, 0x4B
2:  mov dx, DMA_MODE
    out dx, al
    mov al, 3
    mov dx, DMA_MASK
    out dx, al
    clc
    jmp dok
dbad:
    stc
dok:
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

wait:
    push cx
    push dx
    mov cx, 0xFFFF
1:  cmp byte ptr [irqf], 0
    jne 2f
    mov dx, HDC_STAT
    in al, dx
    test al, 0x20
    jnz 2f
    loop 1b
    stc
    jmp 3f
2:  clc
3:  pop dx
    pop cx
    ret

rcsb:
    push dx
    mov dx, HDC_DATA
    in al, dx
    test al, 2
    jz 1f
    stc
    jmp 2f
1:  clc
2:  pop dx
    ret

cmd6:
    push ax
    push bx
    push cx
    push dx
    mov bl, al
    mov cx, 0xFFFF
1:  mov dx, HDC_STAT
    in al, dx
    test al, 4
    jnz 2f
    loop 1b
2:  mov dx, HDC_DATA
    mov al, bl
    out dx, al
    mov al, [dr]
    mov cl, 5
    shl al, cl
    or al, [hd]
    out dx, al
    mov ax, [cy]
    mov cl, 6
    shl ah, cl
    and ah, 0xC0
    mov al, [sec]
    and al, 0x3F
    or al, ah
    out dx, al
    mov al, [cy]
    out dx, al
    mov al, [n]
    out dx, al
    xor al, al
    out dx, al
    pop dx
    pop cx
    pop bx
    pop ax
    ret

md:   .byte 0
dr:   .byte 0
hd:   .byte 0
cy:   .word 0
sec:  .byte 0
n:    .byte 0
irqf: .byte 0
lst:  .byte 0
cfl:  .byte 0
sa:   .word 0
sb:   .word 0
sc:   .word 0
sd:   .word 0
se:   .word 0
o13:  .long 0
/* Host patches these 8 bytes (drive0 then drive1); keep in .text for DS:offset access */
.global geo
geo:
    .byte 0x32, 0x01, 4, 17
    .byte 0, 0, 0, 0

.section .vbuf, "aw", @nobits
vb:
    .space 512
