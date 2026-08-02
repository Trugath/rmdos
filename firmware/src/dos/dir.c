/* DIR.COM — classic-style listing via FindFirst/Next. */
#include "dos.h"

static char dirpat[64];
static char cwd_tmp[64];
static char dta[128];
static int dir_count;
static int dir_bytes_lo;
static int dir_bytes_hi;
static char msg_hdr[16] = " Directory of $";
static char msg_tag[7] = "<DIR>$";
static char msg_fs1[10] = "        $";
static char msg_fs2[15] = " File(s)     $";
static char msg_fs3[30] = " bytes\r\n                    $";
static char msg_fs4[15] = " bytes free\r\n$";
static char msg_nf[18] = "File not found\r\n$";
static char msg_crlf[4] = "\r\n$";

static void build_pattern(void)
{
    int i;
    int c;
    int di;
    int has_wild;

    i = 0;
    if (!args_skip()) {
        buf_set(dirpat, 0, '*');
        buf_set(dirpat, 1, '.');
        buf_set(dirpat, 2, '*');
        buf_set(dirpat, 3, 0);
        return;
    }
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

static void print_entry(void)
{
    int i;
    int c;
    int attr;
    int nam;
    int lo;
    int hi;

    nam = buf_addr(dta, 0x1E);
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
    print_dollar(msg_crlf);
    /* skip . and .. for counts */
    if (peek_byte(nam) == '.' && peek_byte(nam + 1) == 0) {
        return;
    }
    if (peek_byte(nam) == '.' && peek_byte(nam + 1) == '.' && peek_byte(nam + 2) == 0) {
        return;
    }
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

static void print_footer(void)
{
    int free_lo;
    int free_hi;

    print_dollar(msg_fs1);
    print_u32(dir_count, 0);
    print_dollar(msg_fs2);
    print_u32(dir_bytes_lo, dir_bytes_hi);
    print_dollar(msg_fs3);
    if (dos_disk_free(0) == 0) {
        /* free_clusters * secs_per_clust * bytes_per_sect */
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
    dir_count = 0;
    dir_bytes_lo = 0;
    dir_bytes_hi = 0;
    args_init();
    build_pattern();
    dos_set_dta(dta);
    print_header();
    if (dos_find_first(dirpat, 0x10) == -1) {
        print_dollar(msg_nf);
        return 1;
    }
    while (1) {
        print_entry();
        if (dos_find_next() == -1) {
            break;
        }
    }
    print_footer();
    return 0;
}
