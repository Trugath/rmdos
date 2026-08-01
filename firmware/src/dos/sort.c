/* SORT.COM — sort lines from a file (or stdin) to stdout. */
#include "dos.h"

#define LINE_MAX 40
#define MAX_LINES 24

static char pathbuf[64];
static char pool[960];
static char one[2];
static char msg_err[21] = "SORT: open failed\r\n$";
static int nlines;
static int linelen;
static int handle;

static int line_off(int idx)
{
    return idx * LINE_MAX;
}

static int line_cmp(int a, int b)
{
    int i;
    int ca;
    int cb;
    i = 0;
    while (i < LINE_MAX) {
        ca = buf_get(pool, line_off(a) + i);
        cb = buf_get(pool, line_off(b) + i);
        if (ca != cb) {
            if (ca < cb) return -1;
            return 1;
        }
        if (ca == 0) return 0;
        i = i + 1;
    }
    return 0;
}

static void line_swap(int a, int b)
{
    int i;
    int t;
    int oa;
    int ob;
    oa = line_off(a);
    ob = line_off(b);
    i = 0;
    while (i < LINE_MAX) {
        t = buf_get(pool, oa + i);
        buf_set(pool, oa + i, buf_get(pool, ob + i));
        buf_set(pool, ob + i, t);
        i = i + 1;
    }
}

static void finish_line(void)
{
    if (nlines >= MAX_LINES) {
        linelen = 0;
        return;
    }
    buf_set(pool, line_off(nlines) + linelen, 0);
    nlines = nlines + 1;
    linelen = 0;
}

static void ingest(int c)
{
    if (c == 13) {
        return;
    }
    if (c == 10) {
        finish_line();
        return;
    }
    if (linelen < LINE_MAX - 1 && nlines < MAX_LINES) {
        buf_set(pool, line_off(nlines) + linelen, c);
        linelen = linelen + 1;
    }
}

int main(void)
{
    int n;
    int i;
    int j;
    int c;

    nlines = 0;
    linelen = 0;
    args_init();
    if (args_token(pathbuf, 64)) {
        handle = dos_open(pathbuf, 0);
        if (handle == -1) {
            print_dollar(msg_err);
            return 1;
        }
    } else {
        handle = 0;
    }
    while (1) {
        n = dos_read(handle, one, 1);
        if (n == 0 || n == -1) break;
        ingest(buf_get(one, 0));
    }
    if (linelen > 0) {
        finish_line();
    }
    if (handle > 0) {
        dos_close(handle);
    }
    i = 0;
    while (i < nlines) {
        j = i + 1;
        while (j < nlines) {
            if (line_cmp(i, j) > 0) {
                line_swap(i, j);
            }
            j = j + 1;
        }
        i = i + 1;
    }
    i = 0;
    while (i < nlines) {
        j = 0;
        while (1) {
            c = buf_get(pool, line_off(i) + j);
            if (c == 0) break;
            print_char(c);
            j = j + 1;
        }
        print_dollar("\r\n$");
        i = i + 1;
    }
    return 0;
}
