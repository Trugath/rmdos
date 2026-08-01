/* MORE.COM — page text from a file (or stdin handle 0). */
#include "dos.h"

static char pathbuf[64];
static char one[2];
static char msg_more[12] = "-- More --$";
static char msg_erase[14] = "\r          \r$";
static char msg_err[21] = "MORE: open failed\r\n$";

static void pause_more(void)
{
    print_dollar(msg_more);
    read_key();
    print_dollar(msg_erase);
}

int main(void)
{
    int handle;
    int line_count;
    int page_lines;
    int n;
    int c;

    line_count = 0;
    page_lines = screen_page_lines();
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
        if (n == 0 || n == -1) {
            break;
        }
        c = buf_get(one, 0);
        print_char(c);
        if (c == 10) {
            line_count = line_count + 1;
            if (line_count >= page_lines) {
                pause_more();
                line_count = 0;
            }
        }
    }
    if (handle != 0) {
        dos_close(handle);
    }
    return 0;
}
