/* COMP.COM — compare two files (binary). */
#include "dos.h"

#define BUF_SIZE 256

static char path1[64];
static char path2[64];
static char buf1[BUF_SIZE];
static char buf2[BUF_SIZE];
static char msg_usage[24] = "COMP file1 file2\r\n$";
static char msg_err[22] = "COMP: open failed\r\n$";
static char msg_ok[18] = "Files compare OK\r\n$";
static char msg_diff[16] = "Files differ\r\n$";

static int str_same(char *a, char *b)
{
    int i;
    int c;
    i = 0;
    while (1) {
        c = toupper_ch(buf_get(a, i));
        if (c != toupper_ch(buf_get(b, i))) return 0;
        if (c == 0) return 1;
        i = i + 1;
    }
}

int main(void)
{
    int h1;
    int h2;
    int n1;
    int n2;
    int i;

    args_init();
    if (!args_token(path1, 64) || !args_token(path2, 64)) {
        print_dollar(msg_usage);
        return 2;
    }
    if (str_same(path1, path2)) {
        print_dollar(msg_ok);
        return 0;
    }
    h1 = dos_open(path1, 0);
    if (h1 == -1) {
        print_dollar(msg_err);
        return 1;
    }
    h2 = dos_open(path2, 0);
    if (h2 == -1) {
        dos_close(h1);
        print_dollar(msg_err);
        return 1;
    }
    while (1) {
        n1 = dos_read(h1, buf1, BUF_SIZE);
        n2 = dos_read(h2, buf2, BUF_SIZE);
        if (n1 == -1 || n2 == -1) {
            dos_close(h1);
            dos_close(h2);
            print_dollar(msg_err);
            return 1;
        }
        if (n1 == 0 && n2 == 0) {
            dos_close(h1);
            dos_close(h2);
            print_dollar(msg_ok);
            return 0;
        }
        if (n1 != n2) {
            dos_close(h1);
            dos_close(h2);
            print_dollar(msg_diff);
            return 1;
        }
        i = 0;
        while (i < n1) {
            if (buf_get(buf1, i) != buf_get(buf2, i)) {
                dos_close(h1);
                dos_close(h2);
                print_dollar(msg_diff);
                return 1;
            }
            i = i + 1;
        }
    }
}
