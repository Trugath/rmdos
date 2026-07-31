.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * rmDOS KERNEL.SYS — INT 21h + writable FAT12 + tools.
 * Boot leaves DL = drive; entered at 0070:0000.
 */

_start:
    cli
    mov ax, cs
    mov ds, ax
    mov ss, ax
    mov sp, 0xFFFE
    mov [boot_drive], dl
    mov byte ptr [com_active], 0
    mov byte ptr [com_depth], 0
    mov word ptr [current_psp], cs
    mov word ptr [cwd_cluster], 0
    mov byte ptr [cwd_path], 0
    mov word ptr [dta_seg], cs
    lea ax, [default_dta]
    mov word ptr [dta_off], ax
    sti

    call fat12_load_fat
    jc .fat_fail
    call init_std_handles
    call install_dos_vectors
    call mem_init

    mov ax, 0x0003
    int 0x10

    mov ah, 0x09
    lea dx, [msg_banner]
    int 0x21

    /* Silent FAT read self-test */
    mov ah, 0x3D
    xor al, al
    lea dx, [path_kernel]
    int 0x21
    jc .fat_fail
    mov [self_handle], bx

    mov ah, 0x3F
    mov bx, [self_handle]
    mov cx, 5
    lea dx, [read_buf]
    int 0x21
    jc .fat_fail_close

    mov ah, 0x3E
    mov bx, [self_handle]
    int 0x21

    /* Silent R/W self-test: create, write, read, delete */
    mov ah, 0x3C
    xor cx, cx
    lea dx, [path_rw]
    int 0x21
    jc .rw_fail
    mov [self_handle], bx

    mov ah, 0x40
    mov bx, [self_handle]
    mov cx, 5
    lea dx, [rw_payload]
    int 0x21
    jc .rw_fail_close

    mov ah, 0x3E
    mov bx, [self_handle]
    int 0x21

    mov ah, 0x3D
    xor al, al
    lea dx, [path_rw]
    int 0x21
    jc .rw_fail
    mov [self_handle], bx

    mov ah, 0x3F
    mov bx, [self_handle]
    mov cx, 5
    lea dx, [read_buf]
    int 0x21
    jc .rw_fail_close

    mov ah, 0x3E
    mov bx, [self_handle]
    int 0x21

    mov ah, 0x41
    lea dx, [path_rw]
    int 0x21
    jc .rw_fail

    /* Drop straight into the shell */
    lea dx, [path_command]
    call load_and_run_com
    jc .com_fail
    jmp .echo

.fat_fail_close:
    mov ah, 0x3E
    mov bx, [self_handle]
    int 0x21
.fat_fail:
    mov ah, 0x09
    lea dx, [msg_fat_bad]
    int 0x21
    jmp .echo

.rw_fail_close:
    mov ah, 0x3E
    mov bx, [self_handle]
    int 0x21
.rw_fail:
    mov ah, 0x09
    lea dx, [msg_rw_bad]
    int 0x21
    jmp .echo

.com_fail:
    mov ah, 0x09
    lea dx, [msg_com_bad]
    int 0x21

.echo:
    mov ah, 0x01
    int 0x21
    jmp .echo

.include "firmware/src/kernel/inc/console.inc"
.include "firmware/src/kernel/inc/int21.inc"
.include "firmware/src/kernel/inc/fat12.inc"
.include "firmware/src/kernel/inc/path.inc"
.include "firmware/src/kernel/inc/files.inc"
.include "firmware/src/kernel/inc/find.inc"
.include "firmware/src/kernel/inc/memory.inc"
.include "firmware/src/kernel/inc/loader.inc"

.section .data

boot_drive:
    .byte 0
cur_drive:
    .byte 0
com_active:
    .byte 0
com_depth:
    .byte 0
com_err:
    .byte 0
fat_dirty:
    .byte 0
break_flag:
    .byte 1
switchar:
    .byte '/'
child_exit_code:
    .byte 0
child_exit_type:
    .byte 0
exec_pb_valid:
    .byte 0
find_attr:
    .byte 0
self_handle:
    .word 0
com_handle:
    .word 0
psp_run:
    .word 0
current_psp:
    .word 0
path_off:
    .word 0
path_seg:
    .word 0
exec_pb_seg:
    .word 0
exec_pb_off:
    .word 0
dta_seg:
    .word 0
dta_off:
    .word 0
last_dir_lba:
    .word 0
last_dir_idx:
    .word 0
last_size_hi:
    .word 0
cwd_cluster:
    .word 0
path_resolve_cluster:
    .word 0
xfer_buf_off:
    .word 0
xfer_buf_seg:
    .word 0
xfer_want:
    .word 0
xfer_cnt:
    .word 0
xfer_soff:
    .word 0
find_path_off:
    .word 0
find_path_seg:
    .word 0
dos_year:
    .word 2026
dos_month:
    .byte 7
dos_day:
    .byte 31
dos_dow:
    .byte 5
save_ss_tbl:
    .word 0, 0, 0, 0
save_sp_tbl:
    .word 0, 0, 0, 0
psp_tbl:
    .word 0, 0, 0, 0
save_dta_seg_tbl:
    .word 0, 0, 0, 0
save_dta_off_tbl:
    .word 0, 0, 0, 0
first_mcb:
    .word 0
mem_top:
    .word 0

msg_banner:
    .ascii "rmDOS 0.7\r\n$"
msg_fat_bad:
    .ascii "fat fail\r\n$"
msg_rw_bad:
    .ascii "rw fail\r\n$"
msg_com_bad:
    .ascii "com fail\r\n$"
path_kernel:
    .asciz "KERNEL.SYS"
path_rw:
    .asciz "RWTEST.TXT"
path_command:
    .asciz "COMMAND.COM"
rw_payload:
    .ascii "rwok\n"
env_comspec:
    .asciz "COMSPEC=A:\\COMMAND.COM"
env_path:
    .asciz "PATH=A:\\BIN"
country_info:
    .word 0x002E                /* date format */
    .ascii "$"                  /* currency */
    .byte 0, 0, 0, 0, 0, 0, 0
    .ascii ","                  /* thousands */
    .byte 0
    .ascii "."                  /* decimal */
    .byte 0
    .ascii "-"                  /* date sep */
    .byte 0
    .ascii ":"                  /* time sep */
    .byte 0
    .byte 0                    /* currency format */
    .byte 2                    /* currency digits */
    .byte 0                    /* time format */
    .word 0, 0                 /* case map / data */
    .byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

name83:
    .space 11, 0x20
find_pat:
    .space 11, 0x20
find_dirbuf:
    .space 64, 0
cwd_path:
    .space 64, 0
read_buf:
    .space 8, 0
default_dta:
    .space 128, 0

handles:
    .space 128, 0

sector_buf:
    .space 512, 0
sector_guard:
    .byte 0xA5, 0xA5, 0xA5, 0xA5

com_size:
    .word 0
exe_cs:
    .word 0
exe_ip:
    .word 0
exe_ss:
    .word 0
exe_sp:
    .word 0
com_buf:
    .space 16384, 0

fat_buf:
    .space 1536, 0

kernel_end:
