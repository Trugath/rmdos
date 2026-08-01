/* COPY.COM — COPY src dst. */
#include "dos.h"

static char src[64];
static char dst[64];
static char one[2];
static char msg_ok[10] = "copied\r\n$";
static char msg_err[15] = "COPY failed\r\n$";
static char msg_u[16] = "COPY src dst\r\n$";

int main(void)
{
    int hin;
    int hout;
    int n;

    args_init();
    if (!args_token(src, 64) || !args_token(dst, 64)) {
        print_dollar(msg_u);
        return 1;
    }
    hin = dos_open(src, 0);
    if (hin == -1) {
        print_dollar(msg_err);
        return 1;
    }
    hout = dos_create(dst, 0);
    if (hout == -1) {
        dos_close(hin);
        print_dollar(msg_err);
        return 1;
    }
    while (1) {
        n = dos_read(hin, one, 1);
        if (n == 0 || n == -1) {
            break;
        }
        if (dos_write(hout, one, 1) == -1) {
            break;
        }
    }
    dos_close(hout);
    dos_close(hin);
    print_dollar(msg_ok);
    return 0;
}
