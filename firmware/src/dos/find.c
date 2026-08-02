/* FIND.COM — print or count lines selected by a string. */
#include "dos.h"

static char needle[64];
static char pathbuf[64];
static char token[64];
static char linebuf[128];
static char one[2];
static int linelen;
static int found;
static int handle;
static int opt_v;
static int opt_c;
static int opt_n;
static int match_count;
static int line_number;
static char msg_usage[64] = "FIND [/V] [/C] [/N] \"string\" [file]\r\n$";
static char msg_err[24] = "FIND: file error\r\n$";

static int toupper_al(int c)
{
    return toupper_ch(c);
}

static int line_has_needle(void)
{
    int si;
    int di;
    int a;
    int b;

    si = 0;
    while (buf_get(linebuf, si) != 0) {
        di = 0;
        while (1) {
            a = buf_get(needle, di);
            if (a == 0) {
                return 1;
            }
            b = buf_get(linebuf, si + di);
            if (b == 0) {
                break;
            }
            if (toupper_al(a) != toupper_al(b)) {
                break;
            }
            di = di + 1;
        }
        si = si + 1;
    }
    return 0;
}

static void copy_token(char *dst, char *src)
{
    int i;
    int c;

    i = 0;
    while (1) {
        c = buf_get(src, i);
        buf_set(dst, i, c);
        if (c == 0) {
            return;
        }
        i = i + 1;
    }
}

/* Return 0 for an operand, 1 for a valid switch, -1 for an invalid switch. */
static int parse_switch(char *s)
{
    int i;
    int c;

    c = buf_get(s, 0);
    if (c != '/' && c != '-') {
        return 0;
    }
    i = 1;
    if (buf_get(s, i) == 0) {
        return -1;
    }
    while (buf_get(s, i) != 0) {
        c = toupper_al(buf_get(s, i));
        if (c == 'V') {
            opt_v = 1;
        } else if (c == 'C') {
            opt_c = 1;
        } else if (c == 'N') {
            opt_n = 1;
        } else {
            return -1;
        }
        i = i + 1;
    }
    return 1;
}

static void flush_line(void)
{
    int i;
    int selected;

    buf_set(linebuf, linelen, 0);
    line_number = line_number + 1;
    selected = line_has_needle();
    if (opt_v) {
        selected = !selected;
    }
    if (selected) {
        found = 1;
        match_count = match_count + 1;
        if (!opt_c) {
            if (opt_n) {
                print_num(line_number);
                print_char(':');
            }
            i = 0;
            while (buf_get(linebuf, i) != 0) {
                print_char(buf_get(linebuf, i));
                i = i + 1;
            }
            print_char(13);
            print_char(10);
        }
    }
    linelen = 0;
}

int main(void)
{
    int n;
    int c;
    int parsed;
    int have_needle;
    int have_path;
    int last_cr;

    found = 0;
    linelen = 0;
    opt_v = 0;
    opt_c = 0;
    opt_n = 0;
    match_count = 0;
    line_number = 0;
    have_needle = 0;
    have_path = 0;
    last_cr = 0;
    args_init();
    while (args_token_quoted(token, 64)) {
        parsed = parse_switch(token);
        if (parsed < 0) {
            print_dollar(msg_usage);
            return 2;
        }
        if (parsed == 0) {
            if (!have_needle) {
                copy_token(needle, token);
                have_needle = 1;
            } else if (!have_path) {
                copy_token(pathbuf, token);
                have_path = 1;
            } else {
                print_dollar(msg_usage);
                return 2;
            }
        }
    }
    if (!have_needle) {
        print_dollar(msg_usage);
        return 2;
    }
    if (have_path) {
        handle = dos_open(pathbuf, 0);
        if (handle == -1) {
            print_dollar(msg_err);
            return 2;
        }
    } else {
        handle = 0;
    }
    while (1) {
        n = dos_read(handle, one, 1);
        if (n == 0) {
            break;
        }
        if (n == -1) {
            if (handle != 0) {
                dos_close(handle);
            }
            print_dollar(msg_err);
            return 2;
        }
        c = buf_get(one, 0);
        if (c == 13) {
            flush_line();
            last_cr = 1;
        } else if (c == 10) {
            if (!last_cr) {
                flush_line();
            }
            last_cr = 0;
        } else if (linelen < 126) {
            last_cr = 0;
            buf_set(linebuf, linelen, c);
            linelen = linelen + 1;
        } else {
            last_cr = 0;
        }
    }
    if (linelen != 0) {
        flush_line();
    }
    if (handle != 0) {
        dos_close(handle);
    }
    if (opt_c) {
        print_num(match_count);
        print_char(13);
        print_char(10);
    }
    if (found) {
        return 0;
    }
    return 1;
}
