.code16
.intel_syntax noprefix

.include "firmware/bios/inc/equates.inc"

/*
 * rmDOS clean-room XT system BIOS — timer:
 * IRQ0 tick handler and INT 1Ah time-of-day services.
 */

.section .text
.global isr_08, int1a_handler

isr_08:
    push ds
    push ax
    push dx
    mov ax, BDA_SEG
    mov ds, ax
    add word ptr [BDA_TIMER_LO], 1
    adc word ptr [BDA_TIMER_HI], 0
    cmp word ptr [BDA_TIMER_HI], 0x0018
    jb .t08_motor
    cmp word ptr [BDA_TIMER_LO], 0x00B0
    jb .t08_motor
    mov word ptr [BDA_TIMER_LO], 0
    mov word ptr [BDA_TIMER_HI], 0
    mov byte ptr [BDA_TIMER_OFLOW], 1
.t08_motor:
    /* Diskette motor timeout */
    cmp byte ptr [BDA_FLOPPY_TIMEOUT], 0
    je .t08_eoi
    dec byte ptr [BDA_FLOPPY_TIMEOUT]
    jnz .t08_eoi
    /* Kill motors; keep DMA/IRQ + nRESET */
    mov al, 0x0C
    mov dx, PORT_FDC_DOR
    out dx, al
    and byte ptr [BDA_FLOPPY_MOTOR], 0x0F
.t08_eoi:
    mov al, 0x20
    out PORT_PIC_CMD, al
    pop dx
    pop ax
    pop ds
    int 0x1C
    iret

int1a_handler:
    sti
    push ds
    mov bx, BDA_SEG
    mov ds, bx
    cmp ah, 0x00
    je .i1a_get
    cmp ah, 0x01
    je .i1a_set
    pop ds
    iret
.i1a_get:
    mov dx, [BDA_TIMER_LO]
    mov cx, [BDA_TIMER_HI]
    mov al, [BDA_TIMER_OFLOW]
    mov byte ptr [BDA_TIMER_OFLOW], 0
    pop ds
    iret
.i1a_set:
    mov [BDA_TIMER_LO], dx
    mov [BDA_TIMER_HI], cx
    mov byte ptr [BDA_TIMER_OFLOW], 0
    pop ds
    iret
