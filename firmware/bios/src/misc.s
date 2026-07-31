.code16
.intel_syntax noprefix

.include "firmware/bios/inc/equates.inc"

/*
 * rmDOS clean-room XT system BIOS — miscellaneous services:
 * default ISR, equipment/memory/serial/printer stubs, no-BASIC INT 18h,
 * warm-boot entry, F1 error pause.
 */

.section .text
.global isr_default
.global int11_handler, int12_handler, int14_handler, int17_handler, int18_handler
.global cad_main, f1_wait

isr_default:
    iret

int11_handler:
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov ax, [BDA_EQUIP]
    pop ds
    iret

int12_handler:
    push ds
    mov ax, BDA_SEG
    mov ds, ax
    mov ax, [BDA_MEMKB]
    pop ds
    iret

int14_handler:
    /* AH=00 init → AX=0; other: return timeout status in AH */
    cmp ah, 0x00
    je .i14_ok
    mov ah, 0x80
    iret
.i14_ok:
    xor ax, ax
    iret

int17_handler:
    /* printer not present: AH bit0=timeout */
    mov ah, 0x01
    iret

int18_handler:
    push cs
    pop ds
    mov si, offset no_basic_msg
    mov ah, 0x0E
    mov bh, 0
.i18_loop:
    lodsb
    test al, al
    jz .i18_halt
    int 0x10
    jmp .i18_loop
.i18_halt:
    hlt
    jmp .i18_halt

cad_main:
    /* Kept for callers that jump here directly; CAD vector at EA82 is a far JMP. */
    cli
    mov ax, BDA_SEG
    mov ds, ax
    mov word ptr [BDA_WARM_FLAG], WARM_BOOT_MAGIC
    jmp post_main

f1_wait:
    sti
.f1_loop:
    mov ah, 0x00
    int 0x16
    cmp ah, 0x3B                 /* F1 */
    je .f1_done
    cmp ah, 0x01                 /* Esc also dismisses error pause */
    je .f1_done
    jmp .f1_loop
.f1_done:
    ret

no_basic_msg:
    .asciz "rmDOS: no ROM BASIC\r\n"
