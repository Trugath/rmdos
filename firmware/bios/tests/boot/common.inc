/*
 * Shared helpers for BIOS boot-sector unit tests.
 * Linked into a 512-byte sector; keep this compact.
 */

.equ COM1_DATA, 0x3F8
.equ COM1_LSR, 0x3FD
.equ SHUTDOWN_PORT, 0x8900

/* AL = byte → COM1 */
serial_putc:
    push ax
    push dx
    mov ah, al
.sp_wait:
    mov dx, COM1_LSR
    in al, dx
    test al, 0x20
    jz .sp_wait
    mov al, ah
    mov dx, COM1_DATA
    out dx, al
    pop dx
    pop ax
    ret

/* DS:SI = ASCIZ → COM1 */
serial_puts:
    push ax
    push si
.sp_loop:
    lodsb
    test al, al
    jz .sp_done
    call serial_putc
    jmp .sp_loop
.sp_done:
    pop si
    pop ax
    ret

/* Write ASCII "Shutdown" to port 0x8900 */
do_shutdown:
    push ax
    push dx
    push si
    push ds
    push cs
    pop ds
    mov si, offset .shut_str
    mov dx, SHUTDOWN_PORT
.shut_loop:
    lodsb
    test al, al
    jz .shut_done
    out dx, al
    jmp .shut_loop
.shut_done:
    pop ds
    pop si
    pop dx
    pop ax
    ret

.shut_str:
    .asciz "Shutdown"

/*
 * IN: DS:SI = test name (ASCIZ).
 * Prints "PASS name\r\n" then shuts down.
 */
pass_and_halt:
    push ds
    push si
    push cs
    pop ds
    mov si, offset .pass_pfx
    call serial_puts
    pop si
    pop ds
    call serial_puts
    push cs
    pop ds
    mov si, offset .crlf
    call serial_puts
    call do_shutdown
.pass_spin:
    hlt
    jmp .pass_spin

.pass_pfx:
    .asciz "PASS "
.crlf:
    .asciz "\r\n"

/*
 * IN: DS:SI = reason (ASCIZ).
 * Prints "FAIL reason\r\n" then shuts down.
 */
fail_and_halt:
    push ds
    push si
    push cs
    pop ds
    mov si, offset .fail_pfx
    call serial_puts
    pop si
    pop ds
    call serial_puts
    push cs
    pop ds
    mov si, offset .crlf
    call serial_puts
    call do_shutdown
.fail_spin:
    hlt
    jmp .fail_spin

.fail_pfx:
    .asciz "FAIL "
