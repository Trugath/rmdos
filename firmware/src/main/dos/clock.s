.code16
.intel_syntax noprefix

/*
 * CLOCK.COM — read MM58167 RTC @ 2C0h and set DOS date/time (INT 21h AH=2B/2D).
 * ASTCLOCK-style helper when INT 1Ah RTC services are unavailable.
 */

.equ RTC_BASE, 0x2C0

.section .text
.global _start
_start:
    mov dx, RTC_BASE + 1         /* seconds */
    in al, dx
    call bcd_to_bin
    mov bh, al                   /* BH = sec */

    mov dx, RTC_BASE + 2
    in al, dx
    call bcd_to_bin
    mov bl, al                   /* BL = min */

    mov dx, RTC_BASE + 3
    in al, dx
    call bcd_to_bin
    mov ch, al                   /* CH = hour */

    mov dx, RTC_BASE + 5
    in al, dx
    call bcd_to_bin
    mov cl, al                   /* CL = day */

    mov dx, RTC_BASE + 6
    in al, dx
    call bcd_to_bin
    mov dh, al                   /* DH = month */

    mov dx, RTC_BASE + 7
    in al, dx
    call bcd_to_bin              /* AL = year 00-99 */

    /* INT 21h AH=2B set date: CX=year, DH=month, DL=day */
    xor ah, ah
    add ax, 2000
    push cx                      /* save day */
    mov cx, ax                   /* year */
    pop ax
    mov dl, al                   /* day */
    /* DH already month */
    mov ah, 0x2B
    int 0x21

    /* INT 21h AH=2D set time: CH=hour, CL=min, DH=sec, DL=0 */
    mov cl, bl                   /* min */
    mov dh, bh                   /* sec */
    xor dl, dl
    /* CH already hour */
    mov ah, 0x2D
    int 0x21

    mov dx, offset msg
    mov ah, 0x09
    int 0x21
    mov ax, 0x4C00
    int 0x21

bcd_to_bin:
    push bx
    push cx
    mov bl, al
    and bl, 0x0F
    mov cl, 4
    shr al, cl
    mov bh, 10
    mul bh
    add al, bl
    pop cx
    pop bx
    ret

msg:
    .ascii "CLOCK: DOS time set from RTC$"
