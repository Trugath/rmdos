/* 8086-safe conditional jumps for GNU as.
 *
 * With `.code16` and `as --32`, GAS emits 386 Jcc rel16 (`0F 8x`) when a
 * conditional target is out of ±128 bytes. On an 8088, `0F` is POP CS.
 *
 * Expand every Jcc to:  jncc short +3 ; jmp near target
 * The near form (`E9 rel16`) is forced so the short skip is always 3 bytes.
 * (A short `EB` would be only 2 bytes and the +3 skip would land mid-opcode.)
 */

.macro jo target
    .byte 0x71, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jno target
    .byte 0x70, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jb target
    .byte 0x73, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jnae target
    .byte 0x73, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jc target
    .byte 0x73, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jnb target
    .byte 0x72, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jae target
    .byte 0x72, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jnc target
    .byte 0x72, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro je target
    .byte 0x75, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jz target
    .byte 0x75, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jne target
    .byte 0x74, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jnz target
    .byte 0x74, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jbe target
    .byte 0x77, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jna target
    .byte 0x77, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro ja target
    .byte 0x76, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jnbe target
    .byte 0x76, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro js target
    .byte 0x79, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jns target
    .byte 0x78, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jp target
    .byte 0x7b, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jpe target
    .byte 0x7b, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jnp target
    .byte 0x7a, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jpo target
    .byte 0x7a, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jl target
    .byte 0x7d, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jnge target
    .byte 0x7d, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jge target
    .byte 0x7c, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jnl target
    .byte 0x7c, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jle target
    .byte 0x7f, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jng target
    .byte 0x7f, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jg target
    .byte 0x7e, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
.macro jnle target
    .byte 0x7e, 0x03
    .byte 0xe9
    .word \target - (. + 2)
.endm
