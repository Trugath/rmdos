/* DEL.COM — delete an exact name or wildcard set from the PSP tail. */
#include "dos.h"

static char name[64];
static char path[64];
static char dta[128];
static char msg_ok[11] = "deleted\r\n$";
static char msg_err[14] = "DEL failed\r\n$";
static char msg_u[12] = "DEL file\r\n$";

static void build_match_path(void)
{
    int i;
    int base;
    int out;
    int c;
    i = 0;
    base = 0;
    while (1) {
        c = buf_get(name, i);
        if (c == 0) {
            break;
        }
        if (c == '\\' || c == '/' || c == ':') {
            base = i + 1;
        }
        i = i + 1;
    }
    out = 0;
    while (out < base && out < 63) {
        buf_set(path, out, buf_get(name, out));
        out = out + 1;
    }
    i = 0;
    while (buf_get(dta, 0x1E + i) != 0 && out < 63) {
        buf_set(path, out, buf_get(dta, 0x1E + i));
        out = out + 1;
        i = i + 1;
    }
    buf_set(path, out, 0);
}

int main(void)
{
    int deleted;
    int failed;

    args_init();
    if (!args_token(name, 64)) {
        print_dollar(msg_u);
        return 1;
    }
    /* COMMAND.COM owns the interactive *.* prompt; this helper stays batch-safe. */
    dos_set_dta(dta);
    if (dos_find_first(name, 0) == -1) {
        print_dollar(msg_err);
        return 1;
    }
    deleted = 0;
    failed = 0;
    while (1) {
        build_match_path();
        if (dos_delete(path) == -1) {
            failed = 1;
        } else {
            deleted = deleted + 1;
        }
        if (dos_find_next() == -1) {
            break;
        }
    }
    if (deleted > 0) {
        print_dollar(msg_ok);
    }
    if (failed || deleted == 0) {
        print_dollar(msg_err);
        return 1;
    }
    return 0;
}
