/* DIR.COM — classic-style listing via FindFirst/Next; optional /O sort. */
#include "dos.h"

#define DIR_ENT_SIZE 24
#define DIR_MAX_ENTS 80

static char dirpat[64];
static char cwd_tmp[64];
static char dta[128];
static char dir_pool[1920];
static char dir_keys[4];
static char dir_revs[4];
static int dir_count;
static int dir_bytes_lo;
static int dir_bytes_hi;
static int dir_nent;
static int dir_nkeys;
static int opt_w;
static int opt_p;
static int opt_o;
static int wide_col;
static int page_lines;
static char msg_hdr[16] = " Directory of $";
static char msg_tag[7] = "<DIR>$";
static char msg_fs1[10] = "        $";
static char msg_fs2[15] = " File(s)     $";
static char msg_fs3[30] = " bytes\r\n                    $";
static char msg_fs4[15] = " bytes free\r\n$";
static char msg_nf[18] = "File not found\r\n$";
static char msg_inv[18] = "Invalid switch\r\n$";
static char msg_crlf[4] = "\r\n$";
static char msg_more[32] = "Press any key to continue . . .$";

static void print_two_digits(int n)
{
    if (n < 10) {
        print_char('0');
    }
    print_num(n);
}

static void print_dta_datetime(void)
{
    int date;
    int time;
    int month;
    int day;
    int year;
    int hour;
    int minute;

    time = peek_word(buf_addr(dta, 0x16));
    date = peek_word(buf_addr(dta, 0x18));
    day = date & 31;
    month = (date >> 5) & 15;
    year = ((date >> 9) & 127) + 1980;
    hour = (time >> 11) & 31;
    minute = (time >> 5) & 63;

    print_dollar("  $");
    print_two_digits(month);
    print_char('-');
    print_two_digits(day);
    print_char('-');
    print_num(year);
    print_char(' ');
    print_two_digits(hour);
    print_char(':');
    print_two_digits(minute);
}

static int dir_ent_off(int idx)
{
    return idx * DIR_ENT_SIZE;
}

static int dir_u16_cmp(int a, int b)
{
    if (a == b) {
        return 0;
    }
    if ((a & 0x8000) == (b & 0x8000)) {
        if (a < b) {
            return -1;
        }
        return 1;
    }
    if (a & 0x8000) {
        return 1;
    }
    return -1;
}

static int dir_name_cmp(int a, int b)
{
    int i;
    int ca;
    int cb;
    int oa;
    int ob;

    oa = dir_ent_off(a);
    ob = dir_ent_off(b);
    i = 0;
    while (i < 13) {
        ca = buf_get(dir_pool, oa + i);
        cb = buf_get(dir_pool, ob + i);
        if (ca != cb) {
            if (ca < cb) {
                return -1;
            }
            return 1;
        }
        if (ca == 0) {
            return 0;
        }
        i = i + 1;
    }
    return 0;
}

static int dir_ext_at(int off)
{
    int i;
    int c;

    i = 0;
    while (i < 13) {
        c = buf_get(dir_pool, off + i);
        if (c == 0) {
            return off + i;
        }
        if (c == '.') {
            return off + i + 1;
        }
        i = i + 1;
    }
    return off + i;
}

static int dir_ext_cmp(int a, int b)
{
    int i;
    int ca;
    int cb;
    int oa;
    int ob;

    oa = dir_ext_at(dir_ent_off(a));
    ob = dir_ext_at(dir_ent_off(b));
    i = 0;
    while (i < 13) {
        ca = buf_get(dir_pool, oa + i);
        cb = buf_get(dir_pool, ob + i);
        if (ca != cb) {
            if (ca < cb) {
                return -1;
            }
            return 1;
        }
        if (ca == 0) {
            return 0;
        }
        i = i + 1;
    }
    return 0;
}

static int dir_key_cmp(int a, int b, int key)
{
    int oa;
    int ob;
    int da;
    int db;
    int r;

    oa = dir_ent_off(a);
    ob = dir_ent_off(b);
    if (key == 'N') {
        return dir_name_cmp(a, b);
    }
    if (key == 'E') {
        r = dir_ext_cmp(a, b);
        if (r != 0) {
            return r;
        }
        return dir_name_cmp(a, b);
    }
    if (key == 'D') {
        da = peek_word(buf_addr(dir_pool, oa + 16));
        db = peek_word(buf_addr(dir_pool, ob + 16));
        r = dir_u16_cmp(da, db);
        if (r != 0) {
            return r;
        }
        da = peek_word(buf_addr(dir_pool, oa + 14));
        db = peek_word(buf_addr(dir_pool, ob + 14));
        return dir_u16_cmp(da, db);
    }
    if (key == 'S') {
        da = peek_word(buf_addr(dir_pool, oa + 20));
        db = peek_word(buf_addr(dir_pool, ob + 20));
        r = dir_u16_cmp(da, db);
        if (r != 0) {
            return r;
        }
        da = peek_word(buf_addr(dir_pool, oa + 18));
        db = peek_word(buf_addr(dir_pool, ob + 18));
        return dir_u16_cmp(da, db);
    }
    if (key == 'G') {
        da = buf_get(dir_pool, oa + 13) & 0x10;
        db = buf_get(dir_pool, ob + 13) & 0x10;
        if (da != 0 && db == 0) {
            return -1;
        }
        if (da == 0 && db != 0) {
            return 1;
        }
        return 0;
    }
    return 0;
}

static int dir_ent_cmp(int a, int b)
{
    int i;
    int r;
    int key;

    i = 0;
    while (i < dir_nkeys) {
        key = buf_get(dir_keys, i);
        r = dir_key_cmp(a, b, key);
        if (r != 0) {
            if (buf_get(dir_revs, i)) {
                return 0 - r;
            }
            return r;
        }
        i = i + 1;
    }
    return dir_name_cmp(a, b);
}

static void dir_ent_swap(int a, int b)
{
    int i;
    int t;
    int oa;
    int ob;

    oa = dir_ent_off(a);
    ob = dir_ent_off(b);
    i = 0;
    while (i < DIR_ENT_SIZE) {
        t = buf_get(dir_pool, oa + i);
        buf_set(dir_pool, oa + i, buf_get(dir_pool, ob + i));
        buf_set(dir_pool, ob + i, t);
        i = i + 1;
    }
}

static void dir_sort_pool(void)
{
    int i;
    int j;

    i = 0;
    while (i < dir_nent) {
        j = i + 1;
        while (j < dir_nent) {
            if (dir_ent_cmp(i, j) > 0) {
                dir_ent_swap(i, j);
            }
            j = j + 1;
        }
        i = i + 1;
    }
}

static void dir_store_dta(void)
{
    int off;
    int i;
    int c;

    if (dir_nent >= DIR_MAX_ENTS) {
        return;
    }
    off = dir_ent_off(dir_nent);
    i = 0;
    while (i < 13) {
        c = buf_get(dta, 0x1E + i);
        buf_set(dir_pool, off + i, c);
        if (c == 0) {
            break;
        }
        i = i + 1;
    }
    while (i < 13) {
        buf_set(dir_pool, off + i, 0);
        i = i + 1;
    }
    buf_set(dir_pool, off + 13, buf_get(dta, 0x15));
    poke_word(buf_addr(dir_pool, off + 14), peek_word(buf_addr(dta, 0x16)));
    poke_word(buf_addr(dir_pool, off + 16), peek_word(buf_addr(dta, 0x18)));
    poke_word(buf_addr(dir_pool, off + 18), peek_word(buf_addr(dta, 0x1A)));
    poke_word(buf_addr(dir_pool, off + 20), peek_word(buf_addr(dta, 0x1C)));
    dir_nent = dir_nent + 1;
}

static void dir_load_dta(int idx)
{
    int off;
    int i;
    int c;

    off = dir_ent_off(idx);
    i = 0;
    while (i < 13) {
        c = buf_get(dir_pool, off + i);
        buf_set(dta, 0x1E + i, c);
        i = i + 1;
    }
    buf_set(dta, 0x15, buf_get(dir_pool, off + 13));
    poke_word(buf_addr(dta, 0x16), peek_word(buf_addr(dir_pool, off + 14)));
    poke_word(buf_addr(dta, 0x18), peek_word(buf_addr(dir_pool, off + 16)));
    poke_word(buf_addr(dta, 0x1A), peek_word(buf_addr(dir_pool, off + 18)));
    poke_word(buf_addr(dta, 0x1C), peek_word(buf_addr(dir_pool, off + 20)));
}

static void dir_parse_o_keys(void)
{
    int c;
    int rev;

    dir_nkeys = 0;
    c = peek_byte(arg_ptr);
    if (c == ':') {
        arg_ptr = arg_ptr + 1;
        c = peek_byte(arg_ptr);
    }
    while (dir_nkeys < 4) {
        rev = 0;
        c = peek_byte(arg_ptr);
        if (c == '-') {
            rev = 1;
            arg_ptr = arg_ptr + 1;
            c = peek_byte(arg_ptr);
        }
        c = toupper_ch(c);
        if (c == 'N' || c == 'E' || c == 'D' || c == 'S' || c == 'G') {
            buf_set(dir_keys, dir_nkeys, c);
            buf_set(dir_revs, dir_nkeys, rev);
            dir_nkeys = dir_nkeys + 1;
            arg_ptr = arg_ptr + 1;
        } else {
            break;
        }
    }
    if (dir_nkeys == 0) {
        buf_set(dir_keys, 0, 'N');
        buf_set(dir_revs, 0, 0);
        dir_nkeys = 1;
    }
}

static int parse_args(void)
{
    int i;
    int c;
    int di;
    int has_wild;
    int have_pat;

    opt_w = 0;
    opt_p = 0;
    opt_o = 0;
    have_pat = 0;
    dir_nent = 0;
    dir_nkeys = 0;
    buf_set(dirpat, 0, 0);
    args_init();
    while (1) {
        args_skip();
        c = peek_byte(arg_ptr);
        if (c == 0 || c == 13) {
            break;
        }
        if (c == '/') {
            arg_ptr = arg_ptr + 1;
            c = toupper_ch(peek_byte(arg_ptr));
            if (c == 0 || c == 13) {
                print_dollar(msg_inv);
                return -1;
            }
            arg_ptr = arg_ptr + 1;
            if (c == 'W') {
                opt_w = 1;
            } else if (c == 'P') {
                opt_p = 1;
            } else if (c == 'O') {
                opt_o = 1;
                dir_parse_o_keys();
            } else {
                print_dollar(msg_inv);
                return -1;
            }
        } else {
            i = 0;
            while (1) {
                c = peek_byte(arg_ptr);
                if (c == 0 || c == 13 || c == ' ') {
                    break;
                }
                buf_set(dirpat, i, toupper_ch(c));
                i = i + 1;
                arg_ptr = arg_ptr + 1;
            }
            buf_set(dirpat, i, 0);
            have_pat = 1;
        }
    }
    if (!have_pat) {
        buf_set(dirpat, 0, '*');
        buf_set(dirpat, 1, '.');
        buf_set(dirpat, 2, '*');
        buf_set(dirpat, 3, 0);
        return 0;
    }
    has_wild = 0;
    di = 0;
    while (1) {
        c = buf_get(dirpat, di);
        if (c == 0) {
            break;
        }
        if (c == '*' || c == '?' || c == '.') {
            has_wild = 1;
            break;
        }
        di = di + 1;
    }
    if (!has_wild && c == 0) {
        buf_set(dirpat, di, 92);
        di = di + 1;
        buf_set(dirpat, di, '*');
        di = di + 1;
        buf_set(dirpat, di, '.');
        di = di + 1;
        buf_set(dirpat, di, '*');
        di = di + 1;
        buf_set(dirpat, di, 0);
    }
    return 0;
}

static void print_header(void)
{
    int i;
    int last;
    int c;
    int drive;

    print_dollar(msg_hdr);
    last = -1;
    i = 0;
    while (1) {
        c = buf_get(dirpat, i);
        if (c == 0) {
            break;
        }
        if (c == 92) {
            last = i;
        }
        i = i + 1;
    }
    if (last >= 0 && (buf_get(dirpat, 1) == ':' || buf_get(dirpat, 0) == 92)) {
        i = 0;
        while (i < last) {
            print_char(buf_get(dirpat, i));
            i = i + 1;
        }
    } else {
        asm("mov ah, 0x19");
        asm("int 0x21");
        asm("mov ah, 0");
        asm("mov [dos_tmp], ax");
        drive = dos_tmp;
        print_char(drive + 'A');
        print_char(':');
        if (get_cwd(cwd_tmp) == 0) {
            i = 0;
            while (buf_get(cwd_tmp, i) != 0) {
                print_char(buf_get(cwd_tmp, i));
                i = i + 1;
            }
        }
    }
    print_dollar(msg_crlf);
}

static int is_dot_entry(void)
{
    int nam;

    nam = buf_addr(dta, 0x1E);
    if (peek_byte(nam) == '.' && peek_byte(nam + 1) == 0) {
        return 1;
    }
    if (peek_byte(nam) == '.' && peek_byte(nam + 1) == '.' && peek_byte(nam + 2) == 0) {
        return 1;
    }
    return 0;
}

static void count_entry(void)
{
    int attr;
    int lo;
    int hi;

    if (is_dot_entry()) {
        return;
    }
    attr = buf_get(dta, 0x15);
    dir_count = dir_count + 1;
    if (!(attr & 0x10)) {
        lo = peek_word(buf_addr(dta, 0x1A));
        hi = peek_word(buf_addr(dta, 0x1C));
        dir_bytes_lo = dir_bytes_lo + lo;
        dir_bytes_hi = dir_bytes_hi + hi;
        if (dir_bytes_lo < lo) {
            dir_bytes_hi = dir_bytes_hi + 1;
        }
    }
}

static void maybe_page(void)
{
    if (opt_p && page_lines >= 23) {
        print_dollar(msg_more);
        asm("mov ah, 0x08");
        asm("int 0x21");
        print_dollar(msg_crlf);
        page_lines = 0;
    }
}

static void print_entry(void)
{
    int i;
    int c;
    int attr;
    int nam;
    int lo;
    int hi;

    nam = buf_addr(dta, 0x1E);
    if (opt_w) {
        i = 0;
        while (1) {
            c = peek_byte(nam + i);
            if (c == 0) {
                break;
            }
            print_char(c);
            i = i + 1;
        }
        while (i < 13) {
            print_char(' ');
            i = i + 1;
        }
        wide_col = wide_col + 1;
        if (wide_col >= 5) {
            print_dollar(msg_crlf);
            wide_col = 0;
            page_lines = page_lines + 1;
        }
    } else {
        i = 0;
        while (1) {
            c = peek_byte(nam + i);
            if (c == 0) {
                break;
            }
            print_char(c);
            i = i + 1;
        }
        while (i < 13) {
            print_char(' ');
            i = i + 1;
        }
        attr = buf_get(dta, 0x15);
        if (attr & 0x10) {
            print_dollar(msg_tag);
        } else {
            lo = peek_word(buf_addr(dta, 0x1A));
            hi = peek_word(buf_addr(dta, 0x1C));
            print_u32(lo, hi);
        }
        print_dta_datetime();
        print_dollar(msg_crlf);
        page_lines = page_lines + 1;
    }
    count_entry();
    maybe_page();
}

static void print_footer(void)
{
    int free_lo;
    int free_hi;

    if (opt_w && wide_col != 0) {
        print_dollar(msg_crlf);
    }
    print_dollar(msg_fs1);
    print_u32(dir_count, 0);
    print_dollar(msg_fs2);
    print_u32(dir_bytes_lo, dir_bytes_hi);
    print_dollar(msg_fs3);
    if (dos_disk_free(0) == 0) {
        asm("mov ax, [df_ax]");
        asm("mul word ptr [df_bx]");
        asm("mul word ptr [df_cx]");
        asm("mov [dos_tmp], ax");
        asm("mov [df_dx], dx");
        free_lo = dos_tmp;
        free_hi = df_dx;
        print_u32(free_lo, free_hi);
    } else {
        print_u32(0, 0);
    }
    print_dollar(msg_fs4);
}

int main(void)
{
    int i;

    dir_count = 0;
    dir_bytes_lo = 0;
    dir_bytes_hi = 0;
    wide_col = 0;
    page_lines = 1;
    if (parse_args() == -1) {
        return 1;
    }
    dos_set_dta(dta);
    print_header();
    if (dos_find_first(dirpat, 0x10) == -1) {
        print_dollar(msg_nf);
        return 1;
    }
    while (1) {
        if (opt_o) {
            dir_store_dta();
        } else {
            print_entry();
        }
        if (dos_find_next() == -1) {
            break;
        }
    }
    if (opt_o) {
        dir_sort_pool();
        i = 0;
        while (i < dir_nent) {
            dir_load_dta(i);
            print_entry();
            i = i + 1;
        }
    }
    print_footer();
    return 0;
}
