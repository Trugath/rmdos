/* FIND.COM — print lines containing a string. */
#include "dos.h"

static char needle[64];
static char pathbuf[64];
static char linebuf[128];
static char one[2];
static int linelen;
static int found;
static int handle;
static char msg_usage[24] = "FIND \"string\" [file]\r\n$";
static char msg_err[21] = "FIND: open failed\r\n$";

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

static void flush_line(void)
{
    int i;
    if (linelen == 0) {
        return;
    }
    buf_set(linebuf, linelen, 0);
    if (line_has_needle()) {
        found = 1;
        i = 0;
        while (buf_get(linebuf, i) != 0) {
            print_char(buf_get(linebuf, i));
            i = i + 1;
        }
        print_char(13);
        print_char(10);
    }
    linelen = 0;
}

int main(void)
{
    int n;
    int c;

    found = 0;
    linelen = 0;
    args_init();
    if (!args_token_quoted(needle, 64)) {
        print_dollar(msg_usage);
        return 2;
    }
    if (args_token(pathbuf, 64)) {
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
        if (c == 10) {
            flush_line();
        } else if (c == 13) {
            flush_line();
        } else if (linelen < 126) {
            buf_set(linebuf, linelen, c);
            linelen = linelen + 1;
        }
    }
    flush_line();
    if (handle != 0) {
        dos_close(handle);
    }
    if (found) {
        return 0;
    }
    return 1;
}
