/* XCOPY.COM — XCOPY src dst [/S]. */
#include "dos.h"

static char src[64];
static char dst[64];
static char dta[128];
static char buf[128];
static char msg_ok[10] = "copied\r\n$";
static char msg_e[16] = "XCOPY failed\r\n$";
static char msg_u[22] = "XCOPY src dst [/S]\r\n$";

static int last_component(char *path)
{
    int i;
    int last;
    i = 0;
    last = 0;
    while (buf_get(path, i) != 0) {
        if (buf_get(path, i) == 92) {
            last = i + 1;
        }
        i = i + 1;
    }
    return last;
}

static void copy_name_onto(char *path, char *name)
{
    int i;
    int j;
    i = last_component(path);
    j = 0;
    while (1) {
        buf_set(path, i, buf_get(name, j));
        if (buf_get(name, j) == 0) {
            break;
        }
        i = i + 1;
        j = j + 1;
    }
}

int main(void)
{
    int hin;
    int hout;
    int n;
    int tok;

    args_init();
    if (!args_token(src, 64) || !args_token(dst, 64)) {
        print_dollar(msg_u);
        return 1;
    }
    /* Skip optional /S */
    while (args_skip()) {
        tok = peek_byte(arg_ptr);
        if (tok == '/' || tok == '-') {
            arg_ptr = arg_ptr + 1;
            while (1) {
                tok = peek_byte(arg_ptr);
                if (tok == ' ' || tok == 13 || tok == 0) {
                    break;
                }
                arg_ptr = arg_ptr + 1;
            }
        } else {
            break;
        }
    }
    dos_set_dta(dta);
    if (dos_find_first(src, 0x27) == -1) {
        print_dollar(msg_e);
        return 1;
    }
    copy_name_onto(src, buf_addr(dta, 0x1E));
    hin = dos_open(src, 0);
    if (hin == -1) {
        print_dollar(msg_e);
        return 1;
    }
    hout = dos_create(dst, 0);
    if (hout == -1) {
        dos_close(hin);
        print_dollar(msg_e);
        return 1;
    }
    while (1) {
        n = dos_read(hin, buf, 128);
        if (n == 0) {
            break;
        }
        if (n == -1) {
            dos_close(hout);
            dos_close(hin);
            print_dollar(msg_e);
            return 1;
        }
        if (dos_write(hout, buf, n) == -1) {
            dos_close(hout);
            dos_close(hin);
            print_dollar(msg_e);
            return 1;
        }
    }
    dos_close(hout);
    dos_close(hin);
    print_dollar(msg_ok);
    return 0;
}
