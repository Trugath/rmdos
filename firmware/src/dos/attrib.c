/* ATTRIB.COM — [+R|-R] [+A|-A] [+S|-S] [+H|-H] file */
#include "dos.h"

static char pattern[64];
static char dta[128];
static char sep[3] = " $";
static char crlf[4] = "\r\n$";
static char msg_u[47] = "ATTRIB [+R|-R] [+A|-A] [+S|-S] [+H|-H] file\r\n$";
static char msg_e[17] = "ATTRIB failed\r\n$";

static int mask;
static int bits;
static int set_mode;

static void show_attrs(int attr)
{
    if (attr & 1) {
        print_char('R');
    } else {
        print_char('-');
    }
    if (attr & 0x20) {
        print_char('A');
    } else {
        print_char('-');
    }
    if (attr & 4) {
        print_char('S');
    } else {
        print_char('-');
    }
    if (attr & 2) {
        print_char('H');
    } else {
        print_char('-');
    }
}

int main(void)
{
    int c;
    int cl;
    int attr;
    int name_i;
    int p;

    mask = 0;
    bits = 0;
    set_mode = 0;
    buf_set(pattern, 0, 0);
    args_init();
    while (args_skip()) {
        c = peek_byte(arg_ptr);
        if (c == '+' || c == '-') {
            arg_ptr = arg_ptr + 1;
            cl = toupper_ch(peek_byte(arg_ptr));
            arg_ptr = arg_ptr + 1;
            if (cl == 'R') {
                cl = 1;
            } else if (cl == 'A') {
                cl = 0x20;
            } else if (cl == 'S') {
                cl = 4;
            } else if (cl == 'H') {
                cl = 2;
            } else {
                cl = 0;
            }
            if (cl != 0) {
                mask = mask | cl;
                if (c == '+') {
                    bits = bits | cl;
                    set_mode = 1;
                }
            }
        } else {
            name_i = 0;
            while (1) {
                c = peek_byte(arg_ptr);
                if (c == ' ' || c == 13 || c == 0) {
                    break;
                }
                if (name_i < 63) {
                    buf_set(pattern, name_i, c);
                    name_i = name_i + 1;
                }
                arg_ptr = arg_ptr + 1;
            }
            buf_set(pattern, name_i, 0);
        }
    }
    if (buf_get(pattern, 0) == 0) {
        print_dollar(msg_u);
        return 1;
    }
    dos_set_dta(dta);
    if (dos_find_first(pattern, 0x37) == -1) {
        print_dollar(msg_e);
        return 1;
    }
    while (1) {
        attr = buf_get(dta, 0x15);
        if (set_mode) {
            attr = (attr & 0x3F & ~mask) | bits;
            p = buf_addr(dta, 0x1E);
            if (dos_chmod(p, 1, attr) == -1) {
                print_dollar(msg_e);
                return 1;
            }
            attr = buf_get(dta, 0x15);
            attr = (attr & 0x3F & ~mask) | bits;
        }
        show_attrs(buf_get(dta, 0x15));
        print_dollar(sep);
        print_string(buf_addr(dta, 0x1E));
        print_dollar(crlf);
        if (dos_find_next() == -1) {
            break;
        }
    }
    return 0;
}
