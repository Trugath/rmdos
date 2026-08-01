.code16
.intel_syntax noprefix
.section .text
.global _start

/*
 * rmDOS KERNEL.SYS — INT 21h + writable FAT12/FAT16 + tools.
 * Boot leaves DL = drive; entered at 0070:0000.
 */

_start:
    cli
    mov ax, cs
    mov ds, ax
    mov ss, ax
    mov sp, 0xFFFE
    mov [boot_drive], dl
    mov byte ptr [cur_drive], 0
    mov byte ptr [num_drives], 2
    mov word ptr [vol_want_base], 0
    mov byte ptr [com_active], 0
    mov byte ptr [com_depth], 0
    mov word ptr [current_psp], cs
    mov word ptr [cwd_cluster], 0
    mov byte ptr [cwd_path], 0
    mov word ptr [dta_seg], cs
    lea ax, [default_dta]
    mov word ptr [dta_off], ax
    sti

    call dos_rebuild_drivemap
    call dos_bind_boot_drive
    call fat12_init_bpb
    jc .fat_fail
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

    call dos_process_config

    /* Drop into the shell (path_command may be set by SHELL=) */
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
.include "firmware/src/kernel/inc/drivemap.inc"
.include "firmware/src/kernel/inc/int21.inc"
.include "firmware/src/kernel/inc/fat12.inc"
.include "firmware/src/kernel/inc/path.inc"
.include "firmware/src/kernel/inc/files.inc"
.include "firmware/src/kernel/inc/find.inc"
.include "firmware/src/kernel/inc/fcb.inc"
.include "firmware/src/kernel/inc/memory.inc"
.include "firmware/src/kernel/inc/loader.inc"
.include "firmware/src/kernel/inc/config.inc"
.include "firmware/src/kernel/inc/absdisk.inc"
.include "firmware/src/kernel/inc/int2f.inc"

.section .data

boot_drive:
    .byte 0
cur_drive:
    .byte 0
num_drives:
    .byte 1
bpb_spc:
    .byte 1
bpb_fats:
    .byte 2
bpb_media:
    .byte 0xF9
bpb_fat_type:
    .byte 12
bpb_reserved:
    .word 2
bpb_root_ents:
    .word 112
bpb_totsec:
    .word 1440
bpb_totsec_hi:
    .word 0
bpb_spf:
    .word 3
bpb_spt:
    .word 9
bpb_heads:
    .word 2
bpb_fat1_lba:
    .word 2
bpb_fat2_lba:
    .word 5
bpb_root_lba:
    .word 8
bpb_root_secs:
    .word 7
bpb_data_lba:
    .word 15
bpb_max_clust:
    .word 0x592
fat_win_sec:
    .word 0xFFFF
fat_eoc:
    .word 0x0FF8
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
tsr_keep_paras:
    .word 0
tsr_psp:
    .word 0
cfg_files:
    .word 20
cfg_buffers:
    .word 8
cfg_handle:
    .word 0
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
dos_last_error:
    .word 0
dos_country_id:
    .word 1
tmp_name_ctr:
    .word 0
tmp_attrs:
    .word 0
tmp_prefix_end:
    .word 0
tmp_path:
    .space 64, 0
fcb_blk_want:
    .word 0
fcb_blk_done:
    .word 0
path_off:
    .word 0
path_seg:
    .word 0
exec_pb_seg:
    .word 0
exec_pb_off:
    .word 0
ovl_load_seg:
    .word 0
ovl_reloc:
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
path_resolve_drive:
    .byte 0
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
xfer_sec:
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
/* Minimal DOS list-of-lists (AH=52). Offset 0 = first MCB segment. */
dos_sysvars:
    .word 0                      /* +00 first MCB */
    .space 14, 0                 /* +02 .. +0F stubs */
    .byte 0                      /* +10 boot drive (0=A) */
    .space 15, 0

msg_banner:
    .ascii "rmDOS 0.8\r\n$"
msg_fat_bad:
    .ascii "fat fail\r\n$"
msg_rw_bad:
    .ascii "rw fail\r\n$"
msg_com_bad:
    .ascii "com fail\r\n$"
msg_int24:
    .asciz "\r\nAbort, Retry, Ignore? "
path_kernel:
    .asciz "KERNEL.SYS"
path_rw:
    .asciz "RWTEST.TXT"
path_command:
    .asciz "COMMAND.COM"
path_config:
    .asciz "CONFIG.SYS"
cfg_kw_install:
    .asciz "INSTALL"
cfg_kw_device:
    .asciz "DEVICE"
cfg_kw_files:
    .asciz "FILES"
cfg_kw_buffers:
    .asciz "BUFFERS"
cfg_kw_shell:
    .asciz "SHELL"
msg_cfg_install:
    .ascii "CONFIG: INSTALL failed\r\n$"
rw_payload:
    .ascii "rwok\n"
env_comspec:
    .asciz "COMSPEC=A:\\COMMAND.COM"
env_path:
    .asciz "PATH=A:\\BIN"
vol_base_lba:
    .word 0
vol_want_base:
    .word 0
drive_map_bios:
    .space DRIVEMAP_MAX, 0
drive_map_base:
    .space DRIVEMAP_MAX * 2, 0
abs_write:
    .byte 0
abs_saved_drv:
    .byte 0xFF
a57_time:
    .word 0
a57_date:
    .word 0
a57_lba:
    .word 0
wsa_ch:
    .byte 0
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
fcb_path:
    .space 16, 0
fcb_path2:
    .space 16, 0
fcb_pos_lo:
    .word 0
fcb_pos_hi:
    .word 0
fcb_xfer:
    .word 0
fcb_saved_dta_seg:
    .word 0
fcb_saved_dta_off:
    .word 0
fcb_parse_wild:
    .byte 0
fcb_find_dta:
    .space 128, 0
cwd_path:
    .space 64, 0
read_buf:
    .space 8, 0
default_dta:
    .space 128, 0

handles:
    .space 320, 0

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
cfg_ch:
    .byte 0
cfg_line:
    .space 120, 0
com_buf:
    .space 24576, 0

fat_buf:
    .space 1024, 0

kernel_end:
