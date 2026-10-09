.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * EMM.SYS — LIM EMS 3.2 core for k8086 ems-window.
 * Character device EMMXXXX0; INT 67h; I/O 260h–263h; frame D000h.
 * Write FFh to a window port to unmap. Fits DEVICE= SYS_MAX (8 KiB).
 */

.equ DEV_CMD_INIT, 0
.equ DEV_STAT_DONE, 0x0100
.equ DEV_STAT_ERROR, 0x8100

.equ EMS_PORT, 0x260
.equ EMS_FRAME_SEG, 0xD000
.equ EMS_UNMAP, 0xFF
.equ EMS_MAX_HANDLES, 32
.equ EMS_WINDOWS, 4

.equ E_OK, 0x00
.equ E_SOFT, 0x80
.equ E_HANDLE, 0x83
.equ E_FUNC, 0x84
.equ E_NOHAND, 0x85
.equ E_MAPCTX, 0x86
.equ E_MORE, 0x87
.equ E_NOFREE, 0x88
.equ E_ZERO, 0x89
.equ E_LOGPAGE, 0x8A
.equ E_PHYSPAGE, 0x8B

_start:
emm_hdr:
    .word 0xFFFF
    .word 0xFFFF
    .word 0x8000
    .word offset emm_strategy
    .word offset emm_interrupt
    .ascii "EMMXXXX0"

emm_strategy:
    mov word ptr cs:[emm_rh_off], bx
    mov word ptr cs:[emm_rh_seg], es
    retf

emm_interrupt:
    push ax
    push bx
    push ds
    push es
    push cs
    pop ds
    mov es, word ptr [emm_rh_seg]
    mov bx, word ptr [emm_rh_off]
    cmp byte ptr es:[bx + 2], DEV_CMD_INIT
    je .ei_init
    mov word ptr es:[bx + 3], DEV_STAT_DONE
    jmp .ei_done
.ei_init:
    call emm_init
    jc .ei_fail
    lea ax, [emm_image_end]
    mov es:[bx + 0x0E], ax
    mov es:[bx + 0x10], cs
    mov word ptr es:[bx + 3], DEV_STAT_DONE
    jmp .ei_done
.ei_fail:
    mov word ptr es:[bx + 3], DEV_STAT_ERROR
.ei_done:
    pop es
    pop ds
    pop bx
    pop ax
    retf

emm_init:
    push ax
    push bx
    push cx
    push dx
    push di
    push es
    call emm_probe
    jc .emi_bad
    push cs
    pop es
    lea di, [page_owner]
    mov cx, 256
    mov al, 0xFF
    rep stosb
    lea di, [hdl_pages]
    mov cx, EMS_MAX_HANDLES
    xor ax, ax
    rep stosw
    lea di, [hdl_active]
    mov cx, EMS_MAX_HANDLES
    xor al, al
    rep stosb
    lea di, [hdl_saved]
    mov cx, EMS_MAX_HANDLES
    rep stosb
    lea di, [hdl_savemap]
    mov cx, EMS_MAX_HANDLES * EMS_WINDOWS
    mov al, EMS_UNMAP
    rep stosb
    mov ax, word ptr [probed_pages]
    mov word ptr [total_pages], ax
    mov word ptr [free_pages], ax
    mov word ptr [used_handles], 0
    mov dx, EMS_PORT
    mov cx, EMS_WINDOWS
    mov al, EMS_UNMAP
.emi_un:
    out dx, al
    inc dx
    loop .emi_un
    xor ax, ax
    mov es, ax
    cli
    lea ax, [int67_handler]
    mov word ptr es:[0x67 * 4], ax
    mov word ptr es:[0x67 * 4 + 2], cs
    sti
    clc
    jmp .emi_out
.emi_bad:
    stc
.emi_out:
    pop es
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

emm_probe:
    push ax
    push bx
    push dx
    push es
    mov ax, EMS_FRAME_SEG
    mov es, ax
    mov dx, EMS_PORT
    xor al, al
    out dx, al
    mov byte ptr es:[0], 0xA5
    cmp byte ptr es:[0], 0xA5
    jne .ep_fail
    mov al, 1
    out dx, al
    mov byte ptr es:[0], 0x5A
    cmp byte ptr es:[0], 0x5A
    jne .ep_fail
    xor al, al
    out dx, al
    cmp byte ptr es:[0], 0xA5
    jne .ep_fail
    mov al, EMS_UNMAP
    out dx, al
    cmp byte ptr es:[0], 0xFF
    jne .ep_fail
    xor bx, bx
.ep_cnt:
    cmp bx, 255
    jae .ep_done
    mov ax, bx
    out dx, al
    in al, dx
    cmp al, bl
    jne .ep_done
    inc bx
    jmp .ep_cnt
.ep_done:
    cmp bx, 4
    jb .ep_fail
    mov word ptr cs:[probed_pages], bx
    clc
    jmp .ep_out
.ep_fail:
    stc
.ep_out:
    pop es
    pop dx
    pop bx
    pop ax
    ret

/*
 * INT 67h — results returned in AH (status), and function-specific regs.
 * Scratch in CS; exit overlays AX/BX/DX onto the stacked copies.
 */
int67_handler:
    sti
    push es
    push ds
    push bp
    push di
    push si
    push dx
    push cx
    push bx
    push ax

    push cs
    pop ds
    mov byte ptr [s_ah], ah
    mov byte ptr [s_al], al
    mov word ptr [s_bx], bx
    mov word ptr [s_cx], cx
    mov word ptr [s_dx], dx
    mov word ptr [s_si], si
    mov word ptr [s_di], di
    mov word ptr [s_es], es
    mov byte ptr [s_status], E_OK

    cmp ah, 0x40
    je .done
    cmp ah, 0x41
    je .f41
    cmp ah, 0x42
    je .f42
    cmp ah, 0x43
    je .f43
    cmp ah, 0x44
    je .f44
    cmp ah, 0x45
    je .f45
    cmp ah, 0x46
    je .f46
    cmp ah, 0x47
    je .f47
    cmp ah, 0x48
    je .f48
    cmp ah, 0x4B
    je .f4b
    cmp ah, 0x4C
    je .f4c
    cmp ah, 0x4D
    je .f4d
    mov byte ptr [s_status], E_FUNC
    jmp .done

.f41:
    mov word ptr [s_bx], EMS_FRAME_SEG
    jmp .done

.f42:
    mov ax, word ptr [free_pages]
    mov word ptr [s_bx], ax
    mov ax, word ptr [total_pages]
    mov word ptr [s_dx], ax
    jmp .done

.f46:
    mov byte ptr [s_al], 0x32
    jmp .done

.f43:
    mov bx, word ptr [s_bx]
    test bx, bx
    jnz .a43n
    mov byte ptr [s_status], E_ZERO
    jmp .done
.a43n:
    cmp bx, word ptr [total_pages]
    jbe .a43t
    mov byte ptr [s_status], E_MORE
    jmp .done
.a43t:
    cmp bx, word ptr [free_pages]
    jbe .a43f
    mov byte ptr [s_status], E_NOFREE
    jmp .done
.a43f:
    call find_free_handle
    jc .a43nh
    mov cx, bx
    xor di, di
.a43lp:
    jcxz .a43ok
.a43sc:
    cmp di, word ptr [total_pages]
    jae .a43sf
    cmp byte ptr [page_owner + di], 0xFF
    je .a43tk
    inc di
    jmp .a43sc
.a43tk:
    mov ax, si
    mov byte ptr [page_owner + di], al
    inc di
    dec cx
    jmp .a43lp
.a43ok:
    mov byte ptr [hdl_active + si], 1
    mov ax, si
    shl ax, 1
    mov di, ax
    mov ax, word ptr [s_bx]
    mov word ptr [hdl_pages + di], ax
    mov ax, word ptr [free_pages]
    sub ax, word ptr [s_bx]
    mov word ptr [free_pages], ax
    inc word ptr [used_handles]
    mov word ptr [s_dx], si
    jmp .done
.a43nh:
    mov byte ptr [s_status], E_NOHAND
    jmp .done
.a43sf:
    mov byte ptr [s_status], E_SOFT
    jmp .done

.f44:
    mov al, byte ptr [s_al]
    cmp al, EMS_WINDOWS
    jb .m44p
    mov byte ptr [s_status], E_PHYSPAGE
    jmp .done
.m44p:
    mov si, word ptr [s_dx]
    cmp si, EMS_MAX_HANDLES
    jae .m44h
    cmp byte ptr [hdl_active + si], 0
    je .m44h
    mov bx, word ptr [s_bx]
    cmp bx, 0xFFFF
    je .m44u
    mov ax, si
    shl ax, 1
    mov di, ax
    cmp bx, word ptr [hdl_pages + di]
    jae .m44l
    call nth_owned_page
    jc .m44l
    mov dx, EMS_PORT
    xor ah, ah
    mov al, byte ptr [s_al]
    add dx, ax
    mov ax, di
    out dx, al
    jmp .done
.m44u:
    mov dx, EMS_PORT
    xor ah, ah
    mov al, byte ptr [s_al]
    add dx, ax
    mov al, EMS_UNMAP
    out dx, al
    jmp .done
.m44h:
    mov byte ptr [s_status], E_HANDLE
    jmp .done
.m44l:
    mov byte ptr [s_status], E_LOGPAGE
    jmp .done

.f45:
    mov si, word ptr [s_dx]
    cmp si, EMS_MAX_HANDLES
    jae .d45h
    cmp byte ptr [hdl_active + si], 0
    je .d45h
    call unmap_handle_windows
    xor di, di
.d45lp:
    cmp di, word ptr [total_pages]
    jae .d45dn
    mov al, byte ptr [page_owner + di]
    mov bx, si
    cmp al, bl
    jne .d45nx
    mov byte ptr [page_owner + di], 0xFF
    inc word ptr [free_pages]
.d45nx:
    inc di
    jmp .d45lp
.d45dn:
    mov byte ptr [hdl_active + si], 0
    mov ax, si
    shl ax, 1
    mov di, ax
    mov word ptr [hdl_pages + di], 0
    mov byte ptr [hdl_saved + si], 0
    dec word ptr [used_handles]
    jmp .done
.d45h:
    mov byte ptr [s_status], E_HANDLE
    jmp .done

.f47:
    mov si, word ptr [s_dx]
    cmp si, EMS_MAX_HANDLES
    jae .s47h
    cmp byte ptr [hdl_active + si], 0
    je .s47h
    cmp byte ptr [hdl_saved + si], 0
    je .s47ok
    mov byte ptr [s_status], E_MAPCTX
    jmp .done
.s47ok:
    mov ax, si
    mov cl, 2
    shl ax, cl
    mov di, ax
    mov dx, EMS_PORT
    xor bx, bx
.s47lp:
    in al, dx
    mov byte ptr [hdl_savemap + di], al
    inc di
    inc dx
    inc bx
    cmp bx, EMS_WINDOWS
    jb .s47lp
    mov byte ptr [hdl_saved + si], 1
    jmp .done
.s47h:
    mov byte ptr [s_status], E_HANDLE
    jmp .done

.f48:
    mov si, word ptr [s_dx]
    cmp si, EMS_MAX_HANDLES
    jae .r48h
    cmp byte ptr [hdl_active + si], 0
    je .r48h
    cmp byte ptr [hdl_saved + si], 0
    jne .r48ok
    mov byte ptr [s_status], E_MAPCTX
    jmp .done
.r48ok:
    mov ax, si
    mov cl, 2
    shl ax, cl
    mov di, ax
    mov dx, EMS_PORT
    xor bx, bx
.r48lp:
    mov al, byte ptr [hdl_savemap + di]
    out dx, al
    inc di
    inc dx
    inc bx
    cmp bx, EMS_WINDOWS
    jb .r48lp
    mov byte ptr [hdl_saved + si], 0
    jmp .done
.r48h:
    mov byte ptr [s_status], E_HANDLE
    jmp .done

.f4b:
    mov ax, word ptr [used_handles]
    mov word ptr [s_bx], ax
    jmp .done

.f4c:
    mov si, word ptr [s_dx]
    cmp si, EMS_MAX_HANDLES
    jae .g4ch
    cmp byte ptr [hdl_active + si], 0
    je .g4ch
    mov ax, si
    shl ax, 1
    mov di, ax
    mov ax, word ptr [hdl_pages + di]
    mov word ptr [s_bx], ax
    jmp .done
.g4ch:
    mov byte ptr [s_status], E_HANDLE
    jmp .done

.f4d:
    mov es, word ptr [s_es]
    mov di, word ptr [s_di]
    xor si, si
.g4dlp:
    cmp si, EMS_MAX_HANDLES
    jae .g4ddn
    cmp byte ptr [hdl_active + si], 0
    je .g4dnx
    mov ax, si
    stosw
    mov ax, si
    shl ax, 1
    mov bx, ax
    mov ax, word ptr [hdl_pages + bx]
    stosw
.g4dnx:
    inc si
    jmp .g4dlp
.g4ddn:
    mov ax, word ptr [used_handles]
    mov word ptr [s_bx], ax
    jmp .done

.done:
    /* Overlay status/results onto stacked AX/BX/DX, then pop & iret. */
    mov al, byte ptr [s_al]
    mov ah, byte ptr [s_status]
    mov bx, word ptr [s_bx]
    mov dx, word ptr [s_dx]
    mov bp, sp
    /* stack: AX BX CX DX SI DI BP DS ES */
    mov word ptr [bp], ax
    mov word ptr [bp + 2], bx
    mov word ptr [bp + 6], dx
    pop ax
    pop bx
    pop cx
    pop dx
    pop si
    pop di
    pop bp
    pop ds
    pop es
    iret

find_free_handle:
    push ax
    push cx
    xor si, si
    mov cx, EMS_MAX_HANDLES
.ffh:
    cmp byte ptr [hdl_active + si], 0
    je .ffhok
    inc si
    loop .ffh
    pop cx
    pop ax
    stc
    ret
.ffhok:
    pop cx
    pop ax
    clc
    ret

/* SI=handle, BX=0-based logical → DI=physical page. */
nth_owned_page:
    push ax
    push cx
    push bx
    mov cx, bx
    xor di, di
.noplp:
    cmp di, word ptr [total_pages]
    jae .nopfl
    mov al, byte ptr [page_owner + di]
    mov bx, si
    cmp al, bl
    jne .nopnx
    jcxz .nopok
    dec cx
.nopnx:
    inc di
    jmp .noplp
.nopok:
    pop bx
    pop cx
    pop ax
    clc
    ret
.nopfl:
    pop bx
    pop cx
    pop ax
    stc
    ret

unmap_handle_windows:
    push ax
    push bx
    push cx
    push dx
    mov dx, EMS_PORT
    xor cx, cx
.uhw:
    in al, dx
    cmp al, EMS_UNMAP
    je .uhwnx
    xor ah, ah
    mov bx, ax
    cmp bx, word ptr [total_pages]
    jae .uhwun
    mov al, byte ptr [page_owner + bx]
    mov bx, si
    cmp al, bl
    jne .uhwnx
.uhwun:
    mov al, EMS_UNMAP
    out dx, al
.uhwnx:
    inc dx
    inc cx
    cmp cx, EMS_WINDOWS
    jb .uhw
    pop dx
    pop cx
    pop bx
    pop ax
    ret

emm_rh_off:     .word 0
emm_rh_seg:     .word 0
probed_pages:   .word 0
total_pages:    .word 0
free_pages:     .word 0
used_handles:   .word 0

s_ah:           .byte 0
s_al:           .byte 0
s_status:       .byte 0
                .byte 0
s_bx:           .word 0
s_cx:           .word 0
s_dx:           .word 0
s_si:           .word 0
s_di:           .word 0
s_es:           .word 0

page_owner:     .space 256, 0xFF
hdl_pages:      .space EMS_MAX_HANDLES * 2, 0
hdl_active:     .space EMS_MAX_HANDLES, 0
hdl_saved:      .space EMS_MAX_HANDLES, 0
hdl_savemap:    .space EMS_MAX_HANDLES * EMS_WINDOWS, 0xFF

emm_image_end:
