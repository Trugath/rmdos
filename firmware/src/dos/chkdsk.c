/* CHKDSK.COM — BPB/FAT/directory auditor + /F repair via INT 25h/26h. */
#include "dos.h"

#define BITMAP_BYTES 4096
#define DIR_STACK_MAX 64

static char boot[512];
static char sec[512];
static char fatsec[512];
static char wrk[512];
static char bitmap[BITMAP_BYTES];
static int drive;
static int hard_err;
static int warn_fix;
static int want_fix;
static int fixing;
static int fixes_applied;

static int bpb_spc;
static int bpb_reserved;
static int bpb_fats;
static int bpb_root_ents;
static int bpb_totsec;
static int bpb_totsec_hi;
static int bpb_media;
static int bpb_spf;
static int fat1_lba;
static int fat2_lba;
static int root_lba;
static int root_secs;
static int data_lba;
static int max_clust;
static int fat_type;
static int fat_eoc;
static int fat_bad;
static int fat_sec_idx;
static int fat_dirty;

static int file_count;
static int dir_count;
static int file_bytes_lo;
static int file_bytes_hi;
static int dir_bytes_lo;
static int dir_bytes_hi;
static int used_clust;
static int free_clust;
static int lost_clust;
static int cross_links;
static int bad_chains;
static int fat_diff;
static int media_mismatch;
static int dir_skip;
static int bad_dot;
static int vol_label[12];
static int has_label;
static int chk_num;
static int recovered;

static int dirstk[DIR_STACK_MAX];
static int dirstk_par[DIR_STACK_MAX];
static int dirstk_n;
static int cur_dir;
static int cur_parent;
static int saw_dot;
static int saw_dotdot;
static int walk_len;
static int walk_ok;

static char msg_bad_bpb[15] = "Invalid BPB\r\n$";
static char msg_io[19] = "Disk read error\r\n$";
static char msg_vol1[18] = "Volume label is $";
static char msg_vol2[15] = "Volume has no$";
static char msg_vol3[8] = " label$";
static char msg_crlf[4] = "\r\n$";
static char msg_total[27] = " bytes total disk space\r\n$";
static char msg_in_files[22] = " bytes in           $";
static char msg_files[15] = " user files\r\n$";
static char msg_in_dirs[22] = " bytes in           $";
static char msg_dirs[16] = " directories\r\n$";
static char msg_avail[30] = " bytes available on disk\r\n$";
static char msg_unit[34] = " bytes in each allocation unit\r\n$";
static char msg_tot_au[35] = " total allocation units on disk\r\n$";
static char msg_avl_au[39] = " available allocation units on disk\r\n$";
static char msg_fat12[16] = "FAT12 volume\r\n$";
static char msg_fat16[16] = "FAT16 volume\r\n$";
static char msg_lost[18] = " lost clusters\r\n$";
static char msg_cross[24] = " cross-linked chains\r\n$";
static char msg_badc[19] = " bad FAT chains\r\n$";
static char msg_mirr[22] = "FAT copies differ\r\n$";
static char msg_media[22] = "Media ID mismatch\r\n$";
static char msg_dskip[29] = " directory stack overflow\r\n$";
static char msg_dot[22] = " invalid . or ..\r\n$";
static char msg_recv[22] = " recovered files\r\n$";
static char msg_fixed[17] = "fixes applied\r\n$";
static char msg_errors[16] = "errors found\r\n$";
static char msg_ok[13] = "CHKDSK OK\r\n$";
static char msg_fail[17] = "CHKDSK failed\r\n$";

static char chkname[13];

static int abs_start;
static int abs_count;
static int abs_buf;

static int abs_read(int start, int count, char *buf)
{
    abs_start = start;
    abs_count = count;
    abs_buf = buf_addr(buf, 0);
    asm("mov al, [drive]");
    asm("mov cx, [abs_count]");
    asm("mov dx, [abs_start]");
    asm("mov bx, [abs_buf]");
    asm("int 0x25");
    asm("pop dx");
    asm("mov ax, 0");
    asm("jnc Lab_ar_ok");
    asm("mov ax, 0xFFFF");
    asm("Lab_ar_ok:");
    reload_ds();
}

static int abs_write(int start, int count, char *buf)
{
    abs_start = start;
    abs_count = count;
    abs_buf = buf_addr(buf, 0);
    asm("mov al, [drive]");
    asm("mov cx, [abs_count]");
    asm("mov dx, [abs_start]");
    asm("mov bx, [abs_buf]");
    asm("int 0x26");
    asm("pop dx");
    asm("mov ax, 0");
    asm("jnc Lab_aw_ok");
    asm("mov ax, 0xFFFF");
    asm("Lab_aw_ok:");
    reload_ds();
}

static void bit_set(int c)
{
    int bi;
    int bo;
    int v;
    if (c < 2 || c >= max_clust) {
        return;
    }
    bi = c >> 3;
    if (bi >= BITMAP_BYTES) {
        return;
    }
    bo = c & 7;
    v = buf_get(bitmap, bi);
    buf_set(bitmap, bi, v | (1 << bo));
}

static int bit_get(int c)
{
    int bi;
    int bo;
    if (c < 2 || c >= max_clust) {
        return 0;
    }
    bi = c >> 3;
    if (bi >= BITMAP_BYTES) {
        return 0;
    }
    bo = c & 7;
    return (buf_get(bitmap, bi) >> bo) & 1;
}

static void bit_clear_all(void)
{
    int i;
    i = 0;
    while (i < BITMAP_BYTES) {
        buf_set(bitmap, i, 0);
        i = i + 1;
    }
}

static int fat_flush(void)
{
    if (!fat_dirty || fat_sec_idx < 0) {
        return 0;
    }
    if (abs_write(fat1_lba + fat_sec_idx, 1, fatsec) == -1) {
        hard_err = 1;
        return -1;
    }
    if (bpb_fats >= 2) {
        if (abs_write(fat2_lba + fat_sec_idx, 1, fatsec) == -1) {
            hard_err = 1;
            return -1;
        }
    }
    fat_dirty = 0;
    return 0;
}

static int fat_load(int sec_i)
{
    if (sec_i == fat_sec_idx) {
        return 0;
    }
    if (fat_flush() != 0) {
        return -1;
    }
    if (abs_read(fat1_lba + sec_i, 1, fatsec) == -1) {
        hard_err = 1;
        return -1;
    }
    fat_sec_idx = sec_i;
    return 0;
}

static int fat_get(int clust)
{
    int off;
    int sec_i;
    int within;
    int val;
    int odd;
    int b0;
    int b1;

    if (fat_type == 16) {
        off = clust << 1;
        sec_i = off >> 9;
        within = off & 511;
        if (abs_read(fat1_lba + sec_i, 1, fatsec) == -1) {
            hard_err = 1;
            return 0;
        }
        return peek_word(buf_addr(fatsec, within));
    }
    off = clust + (clust >> 1);
    sec_i = off >> 9;
    within = off & 511;
    if (abs_read(fat1_lba + sec_i, 1, fatsec) == -1) {
        hard_err = 1;
        return 0;
    }
    if (within == 511) {
        b0 = buf_get(fatsec, 511);
        if (abs_read(fat1_lba + sec_i + 1, 1, wrk) == -1) {
            hard_err = 1;
            return 0;
        }
        b1 = buf_get(wrk, 0);
        val = b0 | (b1 << 8);
    } else {
        b0 = buf_get(fatsec, within);
        b1 = buf_get(fatsec, within + 1);
        val = b0 | (b1 << 8);
    }
    odd = clust & 1;
    if (odd) {
        return (val >> 4) & 0x0FFF;
    }
    return val & 0x0FFF;
}

static void fat_set(int clust, int value)
{
    int off;
    int sec_i;
    int within;
    int val;
    int odd;
    int next_sec;

    if (fat_type == 16) {
        off = clust << 1;
        sec_i = off >> 9;
        within = off & 511;
        if (fat_load(sec_i) != 0) {
            return;
        }
        poke_word(buf_addr(fatsec, within), value);
        fat_dirty = 1;
        fixes_applied = 1;
        return;
    }
    off = clust + (clust >> 1);
    sec_i = off >> 9;
    within = off & 511;
    odd = clust & 1;
    if (within == 511) {
        if (fat_load(sec_i) != 0) {
            return;
        }
        val = buf_get(fatsec, 511);
        if (fat_flush() != 0) {
            return;
        }
        next_sec = sec_i + 1;
        if (abs_read(fat1_lba + next_sec, 1, wrk) == -1) {
            hard_err = 1;
            return;
        }
        val = val | (buf_get(wrk, 0) << 8);
        if (odd) {
            val = (val & 0x000F) | ((value & 0x0FFF) << 4);
        } else {
            val = (val & 0xF000) | (value & 0x0FFF);
        }
        buf_set(fatsec, 511, val & 0xFF);
        fat_sec_idx = sec_i;
        fat_dirty = 1;
        if (fat_flush() != 0) {
            return;
        }
        buf_set(wrk, 0, (val >> 8) & 0xFF);
        if (abs_write(fat1_lba + next_sec, 1, wrk) == -1) {
            hard_err = 1;
            return;
        }
        if (bpb_fats >= 2) {
            if (abs_write(fat2_lba + next_sec, 1, wrk) == -1) {
                hard_err = 1;
                return;
            }
        }
        fat_sec_idx = -1;
        fixes_applied = 1;
        return;
    }
    if (fat_load(sec_i) != 0) {
        return;
    }
    val = peek_word(buf_addr(fatsec, within));
    if (odd) {
        val = (val & 0x000F) | ((value & 0x0FFF) << 4);
    } else {
        val = (val & 0xF000) | (value & 0x0FFF);
    }
    poke_word(buf_addr(fatsec, within), val);
    fat_dirty = 1;
    fixes_applied = 1;
}

static int parse_bpb(void)
{
    int i;
    int ax;
    int rem;

    if (peek_word(buf_addr(boot, 11)) != 512) {
        return -1;
    }
    bpb_spc = buf_get(boot, 13);
    bpb_reserved = peek_word(buf_addr(boot, 14));
    bpb_fats = buf_get(boot, 16);
    bpb_root_ents = peek_word(buf_addr(boot, 17));
    bpb_totsec = peek_word(buf_addr(boot, 19));
    bpb_totsec_hi = 0;
    if (bpb_totsec == 0) {
        bpb_totsec = peek_word(buf_addr(boot, 32));
        bpb_totsec_hi = peek_word(buf_addr(boot, 34));
    }
    bpb_media = buf_get(boot, 21);
    bpb_spf = peek_word(buf_addr(boot, 22));
    if (bpb_spc == 0 || bpb_fats == 0 || bpb_spf == 0) {
        return -1;
    }
    fat1_lba = bpb_reserved;
    fat2_lba = bpb_reserved + bpb_spf;
    root_lba = bpb_reserved;
    i = 0;
    while (i < bpb_fats) {
        root_lba = root_lba + bpb_spf;
        i = i + 1;
    }
    root_secs = bpb_root_ents >> 4;
    data_lba = root_lba + root_secs;
    ax = bpb_totsec - data_lba;
    if (bpb_totsec < data_lba && bpb_totsec_hi == 0) {
        return -1;
    }
    rem = ax % bpb_spc;
    ax = ax / bpb_spc;
    max_clust = ax + 2;
    fat_type = 12;
    fat_eoc = 0x0FF8;
    fat_bad = 0x0FF7;
    if ((max_clust - 2) >= 4085) {
        fat_type = 16;
        fat_eoc = 0xFFF8;
        fat_bad = 0xFFF7;
    }
    /* Clamp to FAT table capacity (720K images use SPF=3 → 1024 entries). */
    if (fat_type == 16) {
        i = bpb_spf << 8;
    } else {
        i = ((bpb_spf << 9) * 2) / 3;
    }
    if (max_clust > i) {
        max_clust = i;
    }
    fat_sec_idx = -1;
    fat_dirty = 0;
    return 0;
}

static void sync_fat_mirror(void)
{
    int i;
    if (bpb_fats < 2) {
        return;
    }
    if (fat_flush() != 0) {
        return;
    }
    i = 0;
    while (i < bpb_spf) {
        if (abs_read(fat1_lba + i, 1, fatsec) == -1) {
            hard_err = 1;
            return;
        }
        if (abs_write(fat2_lba + i, 1, fatsec) == -1) {
            hard_err = 1;
            return;
        }
        i = i + 1;
    }
    fat_sec_idx = -1;
    fat_diff = 0;
    fixes_applied = 1;
}

static void check_fat_mirror(void)
{
    int i;
    int j;
    if (bpb_fats < 2) {
        return;
    }
    if (fat_flush() != 0) {
        return;
    }
    i = 0;
    while (i < bpb_spf) {
        if (abs_read(fat1_lba + i, 1, fatsec) == -1) {
            hard_err = 1;
            return;
        }
        if (abs_read(fat2_lba + i, 1, sec) == -1) {
            hard_err = 1;
            return;
        }
        j = 0;
        while (j < 512) {
            if (buf_get(fatsec, j) != buf_get(sec, j)) {
                fat_diff = 1;
                warn_fix = 1;
                fat_sec_idx = -1;
                if (fixing) {
                    sync_fat_mirror();
                }
                return;
            }
            j = j + 1;
        }
        i = i + 1;
    }
    fat_sec_idx = -1;
}

static void add_file_bytes(int add_lo, int add_hi)
{
    int nlo;
    nlo = file_bytes_lo + add_lo;
    file_bytes_hi = file_bytes_hi + add_hi;
    if (nlo < file_bytes_lo) {
        file_bytes_hi = file_bytes_hi + 1;
    }
    file_bytes_lo = nlo;
}

static void add_dir_bytes(int add_lo, int add_hi)
{
    int nlo;
    nlo = dir_bytes_lo + add_lo;
    dir_bytes_hi = dir_bytes_hi + add_hi;
    if (nlo < dir_bytes_lo) {
        dir_bytes_hi = dir_bytes_hi + 1;
    }
    dir_bytes_lo = nlo;
}

static int cfs_lo;
static int cfs_hi;
static int cfs_bpc;
static int cfs_quot;

static int size_to_clusters(int sz_lo, int sz_hi, int bpc)
{
    if (sz_lo == 0 && sz_hi == 0) {
        return 0;
    }
    /* ceil(size/bpc) via DX:AX / BX. Do the (size + bpc - 1) add in asm —
     * Small-C int is signed 16-bit, so sz_lo+(bpc-1) overflows for sizes
     * 32257..32767 and falsely reports bad FAT chains. */
    cfs_lo = sz_lo;
    cfs_hi = sz_hi;
    cfs_bpc = bpc;
    asm("mov ax, [cfs_lo]");
    asm("mov dx, [cfs_hi]");
    asm("mov bx, [cfs_bpc]");
    asm("dec bx");
    asm("add ax, bx");
    asm("adc dx, 0");
    asm("mov bx, [cfs_bpc]");
    asm("div bx");
    asm("mov [cfs_quot], ax");
    if (cfs_quot == 0) {
        return 1;
    }
    return cfs_quot;
}

/* need_clust < 0: directory (any length >= 1). need_clust >= 0: file. */
static void walk_chain(int start, int need_clust)
{
    int c;
    int next;
    int prev;
    int steps;
    int is_dir;

    walk_ok = 1;
    walk_len = 0;
    is_dir = 0;
    if (need_clust < 0) {
        is_dir = 1;
        need_clust = 1;
    }
    if (start == 0) {
        if (need_clust > 0) {
            walk_ok = 0;
            bad_chains = bad_chains + 1;
            warn_fix = 1;
        }
        return;
    }
    c = start;
    prev = 0;
    steps = 0;
    while (steps < max_clust) {
        if (c < 2 || c >= max_clust) {
            walk_ok = 0;
            bad_chains = bad_chains + 1;
            warn_fix = 1;
            return;
        }
        if (bit_get(c)) {
            cross_links = cross_links + 1;
            warn_fix = 1;
            if (fixing && prev >= 2) {
                fat_set(prev, fat_eoc);
            }
            return;
        }
        bit_set(c);
        used_clust = used_clust + 1;
        walk_len = walk_len + 1;
        next = fat_get(c);
        if (hard_err) {
            return;
        }
        if (next == fat_bad) {
            walk_ok = 0;
            bad_chains = bad_chains + 1;
            warn_fix = 1;
            return;
        }
        if (next >= fat_eoc) {
            if (walk_len < need_clust) {
                walk_ok = 0;
                bad_chains = bad_chains + 1;
                warn_fix = 1;
            } else if (!is_dir && walk_len > need_clust) {
                walk_ok = 0;
                bad_chains = bad_chains + 1;
                warn_fix = 1;
            }
            return;
        }
        if (next == 0) {
            walk_ok = 0;
            bad_chains = bad_chains + 1;
            warn_fix = 1;
            if (fixing) {
                fat_set(c, fat_eoc);
            }
            return;
        }
        prev = c;
        c = next;
        steps = steps + 1;
    }
    walk_ok = 0;
    bad_chains = bad_chains + 1;
    warn_fix = 1;
}

static int is_dot_ent(int ent)
{
    int c0;
    int c1;
    c0 = buf_get(sec, ent);
    c1 = buf_get(sec, ent + 1);
    if (c0 == '.' && c1 == ' ') {
        return 1;
    }
    if (c0 == '.' && c1 == '.' && buf_get(sec, ent + 2) == ' ') {
        return 2;
    }
    return 0;
}

static void save_label(int ent)
{
    int i;
    int c;
    has_label = 1;
    i = 0;
    while (i < 11) {
        c = buf_get(sec, ent + i);
        vol_label[i] = c;
        i = i + 1;
    }
    vol_label[11] = 0;
}

static int dir_sec_lba;
static int dir_sec_dirty;

static void dir_sec_writeback(void)
{
    if (!dir_sec_dirty) {
        return;
    }
    if (abs_write(dir_sec_lba, 1, sec) == -1) {
        hard_err = 1;
        return;
    }
    dir_sec_dirty = 0;
}

static void process_dirent(int ent)
{
    int attr;
    int start;
    int sz_lo;
    int sz_hi;
    int need;
    int bpc;
    int c0;
    int dot;

    c0 = buf_get(sec, ent);
    if (c0 == 0) {
        return;
    }
    if (c0 == 0xE5) {
        return;
    }
    attr = buf_get(sec, ent + 11);
    if (attr == 0x0F) {
        return;
    }
    if (attr & 0x08) {
        if (!has_label) {
            save_label(ent);
        }
        return;
    }
    start = peek_word(buf_addr(sec, ent + 26));
    sz_lo = peek_word(buf_addr(sec, ent + 28));
    sz_hi = peek_word(buf_addr(sec, ent + 30));
    bpc = bpb_spc << 9;
    if (attr & 0x10) {
        dot = is_dot_ent(ent);
        if (dot == 1) {
            saw_dot = 1;
            if (cur_dir != 0 && start != cur_dir) {
                bad_dot = bad_dot + 1;
                warn_fix = 1;
                if (fixing) {
                    poke_word(buf_addr(sec, ent + 26), cur_dir);
                    dir_sec_dirty = 1;
                    fixes_applied = 1;
                }
            }
            return;
        }
        if (dot == 2) {
            saw_dotdot = 1;
            if (cur_dir != 0 && start != cur_parent) {
                bad_dot = bad_dot + 1;
                warn_fix = 1;
                if (fixing) {
                    poke_word(buf_addr(sec, ent + 26), cur_parent);
                    dir_sec_dirty = 1;
                    fixes_applied = 1;
                }
            }
            return;
        }
        dir_count = dir_count + 1;
        walk_chain(start, -1);
        if (walk_len > 0) {
            cfs_lo = walk_len;
            cfs_hi = bpc;
            asm("mov ax, [cfs_lo]");
            asm("mov bx, [cfs_hi]");
            asm("mul bx");
            asm("mov [cfs_lo], ax");
            asm("mov [cfs_hi], dx");
            add_dir_bytes(cfs_lo, cfs_hi);
        }
        if (start >= 2 && start < max_clust) {
            if (dirstk_n < DIR_STACK_MAX) {
                dirstk[dirstk_n] = start;
                dirstk_par[dirstk_n] = cur_dir;
                dirstk_n = dirstk_n + 1;
            } else {
                dir_skip = dir_skip + 1;
                warn_fix = 1;
            }
        }
        return;
    }
    file_count = file_count + 1;
    add_file_bytes(sz_lo, sz_hi);
    need = size_to_clusters(sz_lo, sz_hi, bpc);
    walk_chain(start, need);
}

static void scan_root(void)
{
    int s;
    int ent;
    int c0;
    cur_dir = 0;
    cur_parent = 0;
    s = 0;
    while (s < root_secs) {
        dir_sec_lba = root_lba + s;
        dir_sec_dirty = 0;
        if (abs_read(dir_sec_lba, 1, sec) == -1) {
            hard_err = 1;
            return;
        }
        ent = 0;
        while (ent < 512) {
            c0 = buf_get(sec, ent);
            if (c0 == 0) {
                dir_sec_writeback();
                return;
            }
            process_dirent(ent);
            if (hard_err) {
                return;
            }
            ent = ent + 32;
        }
        dir_sec_writeback();
        if (hard_err) {
            return;
        }
        s = s + 1;
    }
}

static void scan_subdir(int start, int parent)
{
    int c;
    int next;
    int ent;
    int c0;
    int steps;
    int si;
    int base;

    cur_dir = start;
    cur_parent = parent;
    saw_dot = 0;
    saw_dotdot = 0;
    c = start;
    steps = 0;
    while (steps < max_clust) {
        if (c < 2 || c >= max_clust) {
            return;
        }
        base = data_lba + (c - 2) * bpb_spc;
        si = 0;
        while (si < bpb_spc) {
            dir_sec_lba = base + si;
            dir_sec_dirty = 0;
            if (abs_read(dir_sec_lba, 1, sec) == -1) {
                hard_err = 1;
                return;
            }
            ent = 0;
            while (ent < 512) {
                c0 = buf_get(sec, ent);
                if (c0 == 0) {
                    dir_sec_writeback();
                    if (!saw_dot || !saw_dotdot) {
                        bad_dot = bad_dot + 1;
                        warn_fix = 1;
                    }
                    return;
                }
                process_dirent(ent);
                if (hard_err) {
                    return;
                }
                ent = ent + 32;
            }
            dir_sec_writeback();
            if (hard_err) {
                return;
            }
            si = si + 1;
        }
        next = fat_get(c);
        if (hard_err) {
            return;
        }
        if (next >= fat_eoc) {
            if (!saw_dot || !saw_dotdot) {
                bad_dot = bad_dot + 1;
                warn_fix = 1;
            }
            return;
        }
        if (next < 2 || next == fat_bad) {
            return;
        }
        c = next;
        steps = steps + 1;
    }
}

static void make_chk_name(int n)
{
    int i;
    int d;
    int v;
    buf_set(chkname, 0, 'F');
    buf_set(chkname, 1, 'I');
    buf_set(chkname, 2, 'L');
    buf_set(chkname, 3, 'E');
    v = n;
    i = 7;
    while (i >= 4) {
        d = v % 10;
        buf_set(chkname, i, d + '0');
        v = v / 10;
        i = i - 1;
    }
    buf_set(chkname, 8, '.');
    buf_set(chkname, 9, 'C');
    buf_set(chkname, 10, 'H');
    buf_set(chkname, 11, 'K');
    buf_set(chkname, 12, 0);
}

static int root_add_file(int start, int nclust)
{
    int s;
    int ent;
    int c0;
    int i;
    int bpc;
    int sz_lo;
    int sz_hi;

    bpc = bpb_spc << 9;
    cfs_lo = nclust;
    cfs_hi = bpc;
    asm("mov ax, [cfs_lo]");
    asm("mov bx, [cfs_hi]");
    asm("mul bx");
    asm("mov [cfs_lo], ax");
    asm("mov [cfs_hi], dx");
    sz_lo = cfs_lo;
    sz_hi = cfs_hi;

    make_chk_name(chk_num);
    s = 0;
    while (s < root_secs) {
        if (abs_read(root_lba + s, 1, sec) == -1) {
            hard_err = 1;
            return -1;
        }
        ent = 0;
        while (ent < 512) {
            c0 = buf_get(sec, ent);
            if (c0 == 0 || c0 == 0xE5) {
                i = 0;
                while (i < 11) {
                    buf_set(sec, ent + i, buf_get(chkname, i));
                    i = i + 1;
                }
                buf_set(sec, ent + 11, 0x20);
                i = 12;
                while (i < 26) {
                    buf_set(sec, ent + i, 0);
                    i = i + 1;
                }
                poke_word(buf_addr(sec, ent + 26), start);
                poke_word(buf_addr(sec, ent + 28), sz_lo);
                poke_word(buf_addr(sec, ent + 30), sz_hi);
                if (abs_write(root_lba + s, 1, sec) == -1) {
                    hard_err = 1;
                    return -1;
                }
                chk_num = chk_num + 1;
                recovered = recovered + 1;
                fixes_applied = 1;
                return 0;
            }
            ent = ent + 32;
        }
        s = s + 1;
    }
    return -1;
}

static void scan_orphans(void)
{
    int c;
    int v;
    int head;
    int next;
    int len;
    int steps;
    int i;
    int is_head;

    lost_clust = 0;
    free_clust = 0;
    c = 2;
    while (c < max_clust) {
        v = fat_get(c);
        if (hard_err) {
            return;
        }
        if (v == fat_bad) {
            /* reserved */
        } else if (v != 0 && !bit_get(c)) {
            lost_clust = lost_clust + 1;
            warn_fix = 1;
        } else if (v == 0) {
            free_clust = free_clust + 1;
        }
        c = c + 1;
    }

    if (!fixing || lost_clust == 0) {
        return;
    }

    /* Recover orphan chain heads to FILE####.CHK. */
    c = 2;
    while (c < max_clust) {
        v = fat_get(c);
        if (hard_err) {
            return;
        }
        if (v != 0 && v != fat_bad && !bit_get(c)) {
            is_head = 1;
            i = 2;
            while (i < max_clust) {
                if (i != c) {
                    next = fat_get(i);
                    if (hard_err) {
                        return;
                    }
                    if (next == c) {
                        is_head = 0;
                        i = max_clust;
                    }
                }
                i = i + 1;
            }
            if (is_head) {
                head = c;
                len = 0;
                next = head;
                steps = 0;
                while (steps < max_clust) {
                    if (next < 2 || next >= max_clust) {
                        steps = max_clust;
                    } else if (bit_get(next)) {
                        steps = max_clust;
                    } else {
                        v = fat_get(next);
                        if (hard_err) {
                            return;
                        }
                        if (v == 0) {
                            steps = max_clust;
                        } else {
                            len = len + 1;
                            bit_set(next);
                            if (v == fat_bad || v >= fat_eoc || v < 2) {
                                steps = max_clust;
                            } else {
                                next = v;
                                steps = steps + 1;
                            }
                        }
                    }
                }
                if (len > 0) {
                    root_add_file(head, len);
                }
            }
        }
        c = c + 1;
    }

    /* Free remaining orphans (not claimed by recovery). */
    lost_clust = 0;
    c = 2;
    while (c < max_clust) {
        v = fat_get(c);
        if (hard_err) {
            return;
        }
        if (v != 0 && v != fat_bad && !bit_get(c)) {
            fat_set(c, 0);
            free_clust = free_clust + 1;
        }
        c = c + 1;
    }
}

static void print_label(void)
{
    int i;
    int c;
    if (has_label) {
        print_dollar(msg_vol1);
        i = 0;
        while (i < 11) {
            c = vol_label[i];
            if (c == 0) {
                break;
            }
            if (c != ' ' || i < 8) {
                print_char(c);
            }
            i = i + 1;
        }
        print_dollar(msg_crlf);
    } else {
        print_dollar(msg_vol2);
        print_dollar(msg_vol3);
        print_dollar(msg_crlf);
    }
}

static int rep_lo;
static int rep_hi;
static int rep_bpc;
static int rep_clusters;

static void mul16_to_u32(int a, int b)
{
    rep_lo = a;
    rep_hi = b;
    asm("mov ax, [rep_lo]");
    asm("mov bx, [rep_hi]");
    asm("mul bx");
    asm("mov [rep_lo], ax");
    asm("mov [rep_hi], dx");
}

static void print_report(void)
{
    print_label();
    print_dollar(msg_crlf);

    rep_clusters = max_clust - 2;
    rep_bpc = bpb_spc << 9;
    mul16_to_u32(rep_clusters, rep_bpc);
    print_u32(rep_lo, rep_hi);
    print_dollar(msg_total);

    print_u32(file_bytes_lo, file_bytes_hi);
    print_dollar(msg_in_files);
    print_u32(file_count, 0);
    print_dollar(msg_files);

    print_u32(dir_bytes_lo, dir_bytes_hi);
    print_dollar(msg_in_dirs);
    print_u32(dir_count, 0);
    print_dollar(msg_dirs);

    mul16_to_u32(free_clust, rep_bpc);
    print_u32(rep_lo, rep_hi);
    print_dollar(msg_avail);
    print_dollar(msg_crlf);

    print_u32(rep_bpc, 0);
    print_dollar(msg_unit);
    print_u32(rep_clusters, 0);
    print_dollar(msg_tot_au);
    print_u32(free_clust, 0);
    print_dollar(msg_avl_au);
    print_dollar(msg_crlf);

    if (fat_type == 16) {
        print_dollar(msg_fat16);
    } else {
        print_dollar(msg_fat12);
    }

    if (media_mismatch) {
        print_dollar(msg_media);
    }
    if (fat_diff) {
        print_dollar(msg_mirr);
    }
    if (lost_clust) {
        print_u32(lost_clust, 0);
        print_dollar(msg_lost);
    }
    if (cross_links) {
        print_u32(cross_links, 0);
        print_dollar(msg_cross);
    }
    if (bad_chains) {
        print_u32(bad_chains, 0);
        print_dollar(msg_badc);
    }
    if (dir_skip) {
        print_u32(dir_skip, 0);
        print_dollar(msg_dskip);
    }
    if (bad_dot) {
        print_u32(bad_dot, 0);
        print_dollar(msg_dot);
    }
    if (recovered) {
        print_u32(recovered, 0);
        print_dollar(msg_recv);
    }
}

static void reset_counters(void)
{
    file_count = 0;
    dir_count = 0;
    file_bytes_lo = 0;
    file_bytes_hi = 0;
    dir_bytes_lo = 0;
    dir_bytes_hi = 0;
    used_clust = 0;
    free_clust = 0;
    lost_clust = 0;
    cross_links = 0;
    bad_chains = 0;
    fat_diff = 0;
    media_mismatch = 0;
    dir_skip = 0;
    bad_dot = 0;
    has_label = 0;
    dirstk_n = 0;
    recovered = 0;
    warn_fix = 0;
    bit_clear_all();
    fat_sec_idx = -1;
}

static void run_scan(void)
{
    int i;
    int dir;
    int par;

    i = fat_get(0);
    if (hard_err) {
        return;
    }
    if ((i & 0xFF) != bpb_media) {
        media_mismatch = 1;
        warn_fix = 1;
        if (fixing) {
            if (fat_type == 16) {
                fat_set(0, (i & 0xFF00) | bpb_media);
            } else {
                fat_set(0, (i & 0x0F00) | bpb_media);
            }
            media_mismatch = 0;
        }
    }

    check_fat_mirror();
    if (hard_err) {
        return;
    }

    scan_root();
    if (hard_err) {
        return;
    }

    while (dirstk_n > 0) {
        dirstk_n = dirstk_n - 1;
        dir = dirstk[dirstk_n];
        par = dirstk_par[dirstk_n];
        scan_subdir(dir, par);
        if (hard_err) {
            return;
        }
    }

    scan_orphans();
    if (hard_err) {
        return;
    }

    if (fixing) {
        fat_flush();
    }
}

int main(void)
{
    int c;
    int i;
    int did_fix;

    drive = 0;
    hard_err = 0;
    warn_fix = 0;
    want_fix = 0;
    fixing = 0;
    fixes_applied = 0;
    did_fix = 0;
    chk_num = 0;
    recovered = 0;
    dir_sec_dirty = 0;

    reset_counters();

    args_init();
    while (args_skip()) {
        c = toupper_ch(peek_byte(arg_ptr));
        if (c == '/' || c == '-') {
            arg_ptr = arg_ptr + 1;
            c = toupper_ch(peek_byte(arg_ptr));
            if (c == 'F') {
                want_fix = 1;
            }
            while (1) {
                c = peek_byte(arg_ptr);
                if (c == ' ' || c == 9 || c == 13 || c == 0) {
                    break;
                }
                arg_ptr = arg_ptr + 1;
            }
        } else if (peek_byte(arg_ptr + 1) == ':') {
            drive = c - 'A';
            arg_ptr = arg_ptr + 2;
        } else {
            break;
        }
    }

    if (abs_read(0, 1, boot) == -1) {
        print_dollar(msg_io);
        print_dollar(msg_fail);
        return 1;
    }
    if (parse_bpb() != 0) {
        print_dollar(msg_bad_bpb);
        print_dollar(msg_fail);
        return 1;
    }
    if ((max_clust >> 3) > BITMAP_BYTES) {
        print_dollar(msg_bad_bpb);
        print_dollar(msg_fail);
        return 1;
    }

    fixing = want_fix;
    run_scan();
    if (hard_err) {
        print_dollar(msg_io);
        print_dollar(msg_fail);
        return 1;
    }

    if (want_fix && fixes_applied) {
        did_fix = 1;
        if (fat_diff || bpb_fats >= 2) {
            sync_fat_mirror();
        }
        reset_counters();
        fixing = 0;
        run_scan();
        if (hard_err) {
            print_dollar(msg_io);
            print_dollar(msg_fail);
            return 1;
        }
    }

    print_report();
    if (hard_err) {
        print_dollar(msg_fail);
        return 1;
    }
    if (did_fix) {
        print_dollar(msg_fixed);
    }
    if (warn_fix || lost_clust || cross_links || bad_chains || fat_diff
        || media_mismatch || dir_skip || bad_dot) {
        print_dollar(msg_errors);
        return 1;
    }
    print_dollar(msg_ok);
    return 0;
}
