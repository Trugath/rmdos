/* CHKDSK.COM — BPB/FAT/directory auditor via INT 25h. */
#include "dos.h"

#define BITMAP_BYTES 4096
#define DIR_STACK_MAX 24

static char boot[512];
static char sec[512];
static char fatsec[512];
static char bitmap[BITMAP_BYTES];
static int drive;
static int hard_err;
static int warn_fix;
static int want_fix;

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
static int fat_sec_idx;

static int file_count;
static int dir_count;
static int file_bytes_lo;
static int file_bytes_hi;
static int used_clust;
static int free_clust;
static int lost_clust;
static int cross_links;
static int bad_chains;
static int fat_diff;
static int vol_label[12];
static int has_label;

static int dirstk[DIR_STACK_MAX];
static int dirstk_n;
static int walk_clust;
static int walk_len;
static int walk_ok;

static char msg_bad_bpb[16] = "Invalid BPB\r\n$";
static char msg_io[18] = "Disk read error\r\n$";
static char msg_fix[22] = "/F not supported\r\n$";
static char msg_vol1[17] = "Volume label is $";
static char msg_vol2[14] = "Volume has no$";
static char msg_vol3[8] = " label$";
static char msg_crlf[3] = "\r\n$";
static char msg_total[27] = " bytes total disk space\r\n$";
static char msg_in_files[22] = " bytes in           $";
static char msg_files[13] = " user files\r\n$";
static char msg_in_dirs[22] = " bytes in           $";
static char msg_dirs[12] = " directories\r\n$";
static char msg_avail[30] = " bytes available on disk\r\n$";
static char msg_unit[34] = " bytes in each allocation unit\r\n$";
static char msg_tot_au[34] = " total allocation units on disk\r\n$";
static char msg_avl_au[36] = " available allocation units on disk\r\n$";
static char msg_fat12[13] = "FAT12 volume\r\n$";
static char msg_fat16[13] = "FAT16 volume\r\n$";
static char msg_lost[16] = " lost clusters\r\n$";
static char msg_cross[22] = " cross-linked chains\r\n$";
static char msg_badc[18] = " bad FAT chains\r\n$";
static char msg_mirr[22] = "FAT copies differ\r\n$";
static char msg_ok[13] = "CHKDSK OK\r\n$";
static char msg_fail[17] = "CHKDSK failed\r\n$";
static char msg_spc[2] = " $";

static int abs_read(int start, int count, char *buf)
{
    asm("mov al, [drive]");
    asm("mov cx, [bp+6]");
    asm("mov dx, [bp+8]");
    asm("mov bx, [bp+4]");
    asm("int 0x25");
    asm("pop dx");
    asm("mov ax, 0");
    asm("jnc Lab_ar_ok");
    asm("mov ax, 0xFFFF");
    asm("Lab_ar_ok:");
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

static int fat_get(int clust)
{
    int off;
    int sec_i;
    int within;
    int val;
    int odd;

    if (fat_type == 16) {
        off = clust << 1;
        sec_i = off >> 9;
        within = off & 511;
        if (sec_i != fat_sec_idx) {
            if (abs_read(fat1_lba + sec_i, 1, fatsec) == -1) {
                hard_err = 1;
                return 0;
            }
            fat_sec_idx = sec_i;
        }
        return peek_word(buf_addr(fatsec, within));
    }
    off = clust + (clust >> 1);
    sec_i = off >> 9;
    within = off & 511;
    if (sec_i != fat_sec_idx) {
        if (abs_read(fat1_lba + sec_i, 1, fatsec) == -1) {
            hard_err = 1;
            return 0;
        }
        fat_sec_idx = sec_i;
    }
    if (within == 511) {
        val = buf_get(fatsec, 511);
        if (abs_read(fat1_lba + sec_i + 1, 1, sec) == -1) {
            hard_err = 1;
            return 0;
        }
        val = val | (buf_get(sec, 0) << 8);
    } else {
        val = peek_word(buf_addr(fatsec, within));
    }
    odd = clust & 1;
    if (odd) {
        return (val >> 4) & 0x0FFF;
    }
    return val & 0x0FFF;
}

static int parse_bpb(void)
{
    int i;
    int ax;
    int dx;
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
    /* max_clust = 2 + (totsec - data_lba) / spc */
    ax = bpb_totsec - data_lba;
    dx = bpb_totsec_hi;
    if (bpb_totsec < data_lba && bpb_totsec_hi == 0) {
        return -1;
    }
    /* 16-bit divide for era sizes */
    rem = ax % bpb_spc;
    ax = ax / bpb_spc;
    max_clust = ax + 2;
    fat_type = 12;
    fat_eoc = 0x0FF8;
    if ((max_clust - 2) >= 4085) {
        fat_type = 16;
        fat_eoc = 0xFFF8;
    }
    fat_sec_idx = -1;
    return 0;
}

static void check_fat_mirror(void)
{
    int i;
    int j;
    if (bpb_fats < 2) {
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
                return;
            }
            j = j + 1;
        }
        i = i + 1;
    }
}

static void add_bytes(int lo, int hi)
{
    file_bytes_lo = file_bytes_lo + lo;
    file_bytes_hi = file_bytes_hi + hi;
    if (file_bytes_lo < lo) {
        file_bytes_hi = file_bytes_hi + 1;
    }
}

static void walk_chain(int start, int need_clust)
{
    int c;
    int next;
    int n;
    int steps;

    walk_ok = 1;
    walk_len = 0;
    walk_clust = start;
    if (start == 0) {
        if (need_clust > 0) {
            walk_ok = 0;
            bad_chains = bad_chains + 1;
            warn_fix = 1;
        }
        return;
    }
    c = start;
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
        } else {
            bit_set(c);
            used_clust = used_clust + 1;
        }
        walk_len = walk_len + 1;
        next = fat_get(c);
        if (hard_err) {
            return;
        }
        if (next >= fat_eoc) {
            if (walk_len < need_clust) {
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
            return;
        }
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
        return 1;
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

static void process_dirent(int ent)
{
    int attr;
    int start;
    int sz_lo;
    int sz_hi;
    int need;
    int bpc;
    int c0;

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
        if (is_dot_ent(ent)) {
            return;
        }
        dir_count = dir_count + 1;
        walk_chain(start, 1);
        if (start >= 2 && start < max_clust && dirstk_n < DIR_STACK_MAX) {
            dirstk[dirstk_n] = start;
            dirstk_n = dirstk_n + 1;
        }
        return;
    }
    file_count = file_count + 1;
    add_bytes(sz_lo, sz_hi);
    need = 0;
    if (sz_lo != 0 || sz_hi != 0) {
        need = (sz_lo + bpc - 1) / bpc;
        if (sz_hi != 0) {
            /* size_hi contributes sz_hi * 65536 / bpc ≈ sz_hi * (65536/bpc) */
            need = need + sz_hi * ((32768 / bpc) * 2);
        }
        if (need == 0) {
            need = 1;
        }
    }
    walk_chain(start, need);
}

static void scan_root(void)
{
    int s;
    int ent;
    int c0;
    s = 0;
    while (s < root_secs) {
        if (abs_read(root_lba + s, 1, sec) == -1) {
            hard_err = 1;
            return;
        }
        ent = 0;
        while (ent < 512) {
            c0 = buf_get(sec, ent);
            if (c0 == 0) {
                return;
            }
            process_dirent(ent);
            if (hard_err) {
                return;
            }
            ent = ent + 32;
        }
        s = s + 1;
    }
}

static void scan_subdir(int start)
{
    int c;
    int next;
    int ent;
    int c0;
    int steps;
    int si;
    int base;

    c = start;
    steps = 0;
    while (steps < max_clust) {
        if (c < 2 || c >= max_clust) {
            return;
        }
        base = data_lba + (c - 2) * bpb_spc;
        si = 0;
        while (si < bpb_spc) {
            if (abs_read(base + si, 1, sec) == -1) {
                hard_err = 1;
                return;
            }
            ent = 0;
            while (ent < 512) {
                c0 = buf_get(sec, ent);
                if (c0 == 0) {
                    return;
                }
                process_dirent(ent);
                if (hard_err) {
                    return;
                }
                ent = ent + 32;
            }
            si = si + 1;
        }
        next = fat_get(c);
        if (hard_err) {
            return;
        }
        if (next >= fat_eoc) {
            return;
        }
        if (next < 2) {
            return;
        }
        c = next;
        steps = steps + 1;
    }
}

static void scan_orphans(void)
{
    int c;
    int v;
    c = 2;
    while (c < max_clust) {
        v = fat_get(c);
        if (hard_err) {
            return;
        }
        if (v != 0 && !bit_get(c)) {
            lost_clust = lost_clust + 1;
            warn_fix = 1;
        }
        if (v == 0) {
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

    print_u32(0, 0);
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
}

int main(void)
{
    int c;
    int i;
    int dir;

    drive = 0;
    hard_err = 0;
    warn_fix = 0;
    want_fix = 0;
    file_count = 0;
    dir_count = 0;
    file_bytes_lo = 0;
    file_bytes_hi = 0;
    used_clust = 0;
    free_clust = 0;
    lost_clust = 0;
    cross_links = 0;
    bad_chains = 0;
    fat_diff = 0;
    has_label = 0;
    dirstk_n = 0;

    i = 0;
    while (i < BITMAP_BYTES) {
        buf_set(bitmap, i, 0);
        i = i + 1;
    }

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

    if (want_fix) {
        print_dollar(msg_fix);
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

    /* media ID in FAT[0] */
    i = fat_get(0);
    if (hard_err) {
        print_dollar(msg_io);
        print_dollar(msg_fail);
        return 1;
    }
    if ((i & 0xFF) != bpb_media) {
        warn_fix = 1;
    }

    check_fat_mirror();
    if (hard_err) {
        print_dollar(msg_io);
        print_dollar(msg_fail);
        return 1;
    }

    scan_root();
    if (hard_err) {
        print_dollar(msg_io);
        print_dollar(msg_fail);
        return 1;
    }

    while (dirstk_n > 0) {
        dirstk_n = dirstk_n - 1;
        dir = dirstk[dirstk_n];
        scan_subdir(dir);
        if (hard_err) {
            print_dollar(msg_io);
            print_dollar(msg_fail);
            return 1;
        }
    }

    free_clust = 0;
    scan_orphans();
    if (hard_err) {
        print_dollar(msg_io);
        print_dollar(msg_fail);
        return 1;
    }

    print_report();
    if (hard_err) {
        print_dollar(msg_fail);
        return 1;
    }
    print_dollar(msg_ok);
    return 0;
}
