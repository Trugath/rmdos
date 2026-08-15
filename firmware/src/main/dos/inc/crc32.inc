/*
 * CRC-32 (ISO 3309 / gzip): poly 0xEDB88320, init/final XOR 0xFFFFFFFF.
 *
 * State: crc32_lo / crc32_hi (low/high 16-bit halves).
 * crc32_init   — reset
 * crc32_update — AL = next byte
 * crc32_final  — XOR finalize in place
 */

crc32_init:
    mov word ptr [crc32_lo], 0xFFFF
    mov word ptr [crc32_hi], 0xFFFF
    ret

/* AL = byte */
crc32_update:
    push bx
    push cx
    push dx
    mov bx, word ptr [crc32_lo]
    xor bl, al
    mov cx, 8
.crc_bit:
    mov dx, word ptr [crc32_hi]
    shr dx, 1
    rcr bx, 1
    jnc .crc_nob
    xor bx, 0x8320
    xor dx, 0xEDB8
.crc_nob:
    mov word ptr [crc32_hi], dx
    loop .crc_bit
    mov word ptr [crc32_lo], bx
    pop dx
    pop cx
    pop bx
    ret

crc32_final:
    not word ptr [crc32_lo]
    not word ptr [crc32_hi]
    ret

crc32_lo: .word 0
crc32_hi: .word 0
