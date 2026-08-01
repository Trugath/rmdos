/* FC.COM — compare two files (binary). */
#include "dos.h"

static char path1[64];
static char path2[64];
static char buf[256];
static char one[2];
static char msg_usage[22] = "FC file1 file2\r\n$";
static char msg_err[20] = "FC: open failed\r\n$";
static char msg_ok[22] = "Files compare OK\r\n$";
static char msg_diff[18] = "Files differ\r\n$";
static char msg_big[24] = "FC: file too large\r\n$";
static int len1;

int main(void)
{
    int h;
    int n;
    int i;
    int c1;
    int c2;

    args_init();
    if (!args_token(path1, 64) || !args_token(path2, 64)) {
        print_dollar(msg_usage);
        return 2;
    }
    h = dos_open(path1, 0);
    if (h == -1) {
        print_dollar(msg_err);
        return 1;
    }
    len1 = 0;
    while (1) {
        n = dos_read(h, one, 1);
        if (n == 0) break;
        if (n == -1 || len1 >= 255) {
            dos_close(h);
            print_dollar(msg_big);
            return 1;
        }
        buf_set(buf, len1, buf_get(one, 0));
        len1 = len1 + 1;
    }
    dos_close(h);

    h = dos_open(path2, 0);
    if (h == -1) {
        print_dollar(msg_err);
        return 1;
    }
    i = 0;
    while (1) {
        n = dos_read(h, one, 1);
        if (n == 0) break;
        if (n == -1) {
            dos_close(h);
            print_dollar(msg_err);
            return 1;
        }
        if (i >= len1) {
            dos_close(h);
            print_dollar(msg_diff);
            return 1;
        }
        c1 = buf_get(buf, i);
        c2 = buf_get(one, 0);
        if (c1 != c2) {
            dos_close(h);
            print_dollar(msg_diff);
            return 1;
        }
        i = i + 1;
    }
    dos_close(h);
    if (i != len1) {
        print_dollar(msg_diff);
        return 1;
    }
    print_dollar(msg_ok);
    return 0;
}
