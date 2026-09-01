/* DIR.COM — classic-style listing via FindFirst/Next; optional /O sort. */
#include "dos.h"
#include "dirlist.h"

static char dirpat[64];
static char cwd_tmp[64];
static int dir_count = 0;
static int dir_bytes_lo = 0;
static int dir_bytes_hi = 0;
static int opt_w;
static int opt_p;
static int opt_o;
static int free_mul_a;
static int free_mul_b;
static int free_mul_c;
static int free_mul_lo;
static int free_mul_hi;
static int wide_col = 0;
static int page_lines = 1;
static char msg_hdr[16] = " Directory of $";
static char msg_tag[7] = "<DIR>$";
static char msg_fs1[10] = "        $";
static char msg_fs2[15] = " File(s)     $";
static char msg_fs3[30] = " bytes\r\n                    $";
static char msg_fs4[15] = " bytes free\r\n$";
static char msg_nf[18] = "File not found\r\n$";
static char msg_inv[18] = "Invalid switch\r\n$";
static char msg_crlf[4] = "\r\n$";
static char msg_more[33] = "Press any key to continue . . .$";

static void print_two_digits(int n)
{
    if (n < 10) {
        print_char('0');
    }
    print_num(n);
}

static void print_dta_datetime(void)
{
    int time = peek_word(buf_addr(dir_dta, 0x16));
    int date = peek_word(buf_addr(dir_dta, 0x18));
    int day = date & 31;
    int month = (date >> 5) & 15;
    int year = ((date >> 9) & 127) + 1980;
    int hour = (time >> 11) & 31;
    int minute = (time >> 5) & 63;

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
    int have_pat = 0;

    opt_w = 0;
    opt_p = 0;
    opt_o = 0;
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
    char *nam;

    nam = buf_addr(dir_dta, 0x1E);
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
    attr = buf_get(dir_dta, 0x15);
    dir_count = dir_count + 1;
    if (!(attr & FA_DIRENT)) {
        lo = peek_word(buf_addr(dir_dta, 0x1A));
        hi = peek_word(buf_addr(dir_dta, 0x1C));
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
    char *nam;
    int lo;
    int hi;

    nam = buf_addr(dir_dta, 0x1E);
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
        attr = buf_get(dir_dta, 0x15);
        if (attr & FA_DIRENT) {
            print_dollar(msg_tag);
        } else {
            lo = peek_word(buf_addr(dir_dta, 0x1A));
            hi = peek_word(buf_addr(dir_dta, 0x1C));
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
    struct DiskFree df;

    if (opt_w && wide_col != 0) {
        print_dollar(msg_crlf);
    }
    print_dollar(msg_fs1);
    print_u32(dir_count, 0);
    print_dollar(msg_fs2);
    print_u32(dir_bytes_lo, dir_bytes_hi);
    print_dollar(msg_fs3);
    if (dos_disk_free(0, &df) == 0) {
        free_mul_a = df.free_clusters;
        free_mul_b = df.secs_per_clust;
        free_mul_c = df.bytes_per_sect;
        asm("mov ax, [free_mul_a]");
        asm("mul word ptr [free_mul_b]");
        asm("mul word ptr [free_mul_c]");
        asm("mov [free_mul_lo], ax");
        asm("mov [free_mul_hi], dx");
        print_u32(free_mul_lo, free_mul_hi);
    } else {
        print_u32(0, 0);
    }
    print_dollar(msg_fs4);
}

int main(void)
{
    int i;

    if (parse_args() == -1) {
        return 1;
    }
    dos_set_dta(dir_dta);
    print_header();
    if (dos_find_first(dirpat, FA_DIRENT) == -1) {
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
