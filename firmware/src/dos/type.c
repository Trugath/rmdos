/* TYPE.COM — print a text file. */
#include "dos.h"

static char pathbuf[64];
static char one[2];
static char msg_usage[13] = "TYPE file\r\n$";
static char msg_err[21] = "TYPE: open failed\r\n$";

int main(void)
{
    int h;
    int n;

    args_init();
    if (!args_token(pathbuf, 64)) {
        print_dollar(msg_usage);
        return 1;
    }
    h = dos_open(pathbuf, 0);
    if (h == -1) {
        print_dollar(msg_err);
        return 1;
    }
    while (1) {
        n = dos_read(h, one, 1);
        if (n == 0 || n == -1) {
            break;
        }
        print_char(buf_get(one, 0));
    }
    dos_close(h);
    return 0;
}
