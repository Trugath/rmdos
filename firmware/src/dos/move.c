/* MOVE.COM — rename, or copy+delete across volumes. */
#include "dos.h"

static char src[64];
static char dst[64];
static char buf[128];
static char msg_ok[9] = "moved\r\n$";
static char msg_e[15] = "MOVE failed\r\n$";
static char msg_u[16] = "MOVE src dst\r\n$";

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
    if (dos_rename(src, dst) == 0) {
        print_dollar(msg_ok);
        return 0;
    }
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
    if (dos_delete(src) == -1) {
        print_dollar(msg_e);
        return 1;
    }
    print_dollar(msg_ok);
    return 0;
}
