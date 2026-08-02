.code16
.intel_syntax noprefix
.section .text
.global _start

/* INT 10h AH=06/07 scroll + AH=09 write char/attr */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ax, 0x0003
    int 0x10

    /* clear screen via AH=06 AL=0 */
    mov ax, 0x0600
    mov bh, 0x07
    xor cx, cx
    mov dx, 0x184F
    int 0x10

    /* write 'A' at 0,0 via AH=09 */
    mov ah, 0x02
    xor bh, bh
    xor dx, dx
    int 0x10
    mov ah, 0x09
    mov al, 'A'
    mov bh, 0
    mov bl, 0x07
    mov cx, 1
    int 0x10

    /* write 'B' at 1,0 */
    mov ah, 0x02
    mov dx, 0x0100
    int 0x10
    mov ah, 0x09
    mov al, 'B'
    mov cx, 1
    int 0x10

    /* scroll up 1 line full screen */
    mov ax, 0x0601
    mov bh, 0x07
    xor cx, cx
    mov dx, 0x184F
    int 0x10

    mov ax, 0xB800
    mov es, ax
    /* row0 should now be former row1 = 'B' */
    cmp byte ptr es:[0], 'B'
    jne .fail_up
    /* former row0 'A' should be gone from row0 */
    /* row1 blank */
    cmp byte ptr es:[160], ' '
    jne .fail_up_blank

    /* put 'C' at bottom row 24 */
    mov ah, 0x02
    mov dx, 0x1800
    int 0x10
    mov ah, 0x09
    mov al, 'C'
    mov cx, 1
    int 0x10

    /* scroll down 1 line */
    mov ax, 0x0701
    mov bh, 0x07
    xor cx, cx
    mov dx, 0x184F
    int 0x10

    /* row1 should be former row0 'B' */
    cmp byte ptr es:[160], 'B'
    jne .fail_down

    /*
     * Partial window cols 1..10 rows 0..2: poke X/Y/Z then AH=06 up 1.
     * (1,1)←Y, (1,0) Z survives, (2,1) blank.
     */
    mov word ptr es:[162], 0x0758     /* (1,1)='X' */
    mov word ptr es:[322], 0x0759     /* (2,1)='Y' */
    mov word ptr es:[160], 0x075A     /* (1,0)='Z' */
    mov ax, 0x0601
    mov bh, 0x07
    mov cx, 0x0001
    mov dx, 0x020A
    int 0x10
    cmp byte ptr es:[162], 'Y'
    jne .fail_part
    cmp byte ptr es:[160], 'Z'
    jne .fail_part
    cmp byte ptr es:[322], ' '
    jne .fail_part

    push cs
    pop ds
    mov si, offset name
    call pass_and_halt

.fail_up:
    push cs
    pop ds
    mov si, offset msg_up
    call fail_and_halt
.fail_up_blank:
    push cs
    pop ds
    mov si, offset msg_blank
    call fail_and_halt
.fail_down:
    push cs
    pop ds
    mov si, offset msg_down
    call fail_and_halt
.fail_part:
    push cs
    pop ds
    mov si, offset msg_part
    call fail_and_halt

name:
    .asciz "bt_scroll"
msg_up:
    .asciz "bt_scroll:up"
msg_blank:
    .asciz "bt_scroll:blank"
msg_down:
    .asciz "bt_scroll:down"
msg_part:
    .asciz "bt_scroll:part"

.include "firmware/bios/tests/boot/common.inc"
