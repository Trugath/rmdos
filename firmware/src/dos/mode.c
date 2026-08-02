/* MODE.COM — COM1 serial setup via INT 14h; LPT/CON stubs. */
#include "dos.h"

static char tok[32];
static char msg_u[80] = "MODE 40|80|BW80|CO80|CON|COM1: baud,parity,data,stop\r\n$";
static char msg_ok[12] = "MODE OK\r\n$";
static char msg_bad[22] = "MODE: bad args\r\n$";
static char msg_set[28] = "COM1 set: $";
static char msg_lpt[18] = "LPT1 ready\r\n$";
static char msg_lpt_r[25] = "Redirect not supported\r\n$";
static char msg_con[20] = "CON cols=$";
static char msg_crlf[3] = "\r\n$";
static int i14_ax;
static int i14_dx;
static int parse_val;
static int con_cols;

static int token_is(char *s, char *want)
{
    int i;
    int a;
    int b;

    i = 0;
    while (1) {
        a = toupper_ch(buf_get(s, i));
        b = toupper_ch(buf_get(want, i));
        if (a != b) {
            return 0;
        }
        if (a == 0) {
            return 1;
        }
        i = i + 1;
    }
}

static void set_video_mode(int mode)
{
    asm("mov ax, [bp+4]");
    asm("xor ah, ah");
    asm("int 0x10");
    reload_ds();
}

static int parse_num(char *s)
{
    int i;
    int v;
    int c;
    i = 0;
    v = 0;
    if (buf_get(s, 0) == 0) {
        return 0;
    }
    while (1) {
        c = buf_get(s, i);
        if (c == 0) {
            break;
        }
        if (c < '0' || c > '9') {
            return 0;
        }
        v = v * 10 + (c - '0');
        i = i + 1;
    }
    parse_val = v;
    return 1;
}

static int baud_code(int baud)
{
    if (baud == 110) {
        return 0;
    }
    if (baud == 150) {
        return 1;
    }
    if (baud == 300) {
        return 2;
    }
    if (baud == 600) {
        return 3;
    }
    if (baud == 1200) {
        return 4;
    }
    if (baud == 2400) {
        return 5;
    }
    if (baud == 4800) {
        return 6;
    }
    if (baud == 9600) {
        return 7;
    }
    return -1;
}

static int split_csv(char *s, char *a, char *b, char *c, char *d)
{
    int i;
    int n;
    int part;
    int ci;
    i = 0;
    part = 0;
    ci = 0;
    buf_set(a, 0, 0);
    buf_set(b, 0, 0);
    buf_set(c, 0, 0);
    buf_set(d, 0, 0);
    while (1) {
        n = buf_get(s, i);
        if (n == 0) {
            if (part == 0) {
                buf_set(a, ci, 0);
            } else if (part == 1) {
                buf_set(b, ci, 0);
            } else if (part == 2) {
                buf_set(c, ci, 0);
            } else {
                buf_set(d, ci, 0);
            }
            break;
        }
        if (n == ',') {
            if (part == 0) {
                buf_set(a, ci, 0);
            } else if (part == 1) {
                buf_set(b, ci, 0);
            } else if (part == 2) {
                buf_set(c, ci, 0);
            } else {
                return 0;
            }
            part = part + 1;
            ci = 0;
        } else {
            if (ci < 15) {
                if (part == 0) {
                    buf_set(a, ci, n);
                } else if (part == 1) {
                    buf_set(b, ci, n);
                } else if (part == 2) {
                    buf_set(c, ci, n);
                } else {
                    buf_set(d, ci, n);
                }
                ci = ci + 1;
            }
        }
        i = i + 1;
    }
    if (part == 3) {
        return 1;
    }
    return 0;
}

static int mode_com1(char *params)
{
    char a[16];
    char b[16];
    char c[16];
    char d[16];
    int baud;
    int bc;
    int parity;
    int data;
    int stop;
    int al;
    int p;
    int dn;
    int stop_show;
    if (!split_csv(params, a, b, c, d)) {
        return 0;
    }
    if (!parse_num(a)) {
        return 0;
    }
    baud = parse_val;
    bc = baud_code(baud);
    if (bc < 0) {
        return 0;
    }
    p = toupper_ch(buf_get(b, 0));
    if (p == 'N') {
        parity = 0;
    } else if (p == 'O') {
        parity = 1;
    } else if (p == 'E') {
        parity = 2;
    } else {
        return 0;
    }
    if (!parse_num(c)) {
        return 0;
    }
    data = parse_val;
    if (data == 7) {
        dn = 2;
    } else if (data == 8) {
        dn = 3;
    } else {
        return 0;
    }
    if (!parse_num(d)) {
        return 0;
    }
    stop = parse_val;
    stop_show = stop;
    if (stop == 1) {
        stop = 0;
    } else if (stop == 2) {
        stop = 1;
    } else {
        return 0;
    }
    al = (bc << 5) | (parity << 3) | (stop << 2) | dn;
    i14_ax = al & 0xFF;
    i14_dx = 0;
    asm("mov ah, 0");
    asm("mov al, byte ptr [i14_ax]");
    asm("mov dx, [i14_dx]");
    asm("int 0x14");
    reload_ds();
    print_dollar(msg_set);
    print_num(baud);
    print_char(',');
    print_char(p);
    print_char(',');
    print_num(data);
    print_char(',');
    print_num(stop_show);
    print_dollar(msg_crlf);
    print_dollar(msg_ok);
    return 1;
}

int main(void)
{
    char p1[32];
    char p2[32];
    int i;
    int c;

    args_init();
    if (!args_skip() || !args_token(p1, 32)) {
        print_dollar(msg_u);
        return 1;
    }

    i = 0;
    while (buf_get(p1, i) != 0) {
        i = i + 1;
    }
    if (i > 0 && buf_get(p1, i - 1) == ':') {
        buf_set(p1, i - 1, 0);
    }

    if (token_is(p1, "40") || token_is(p1, "80")
        || token_is(p1, "BW80") || token_is(p1, "CO80")) {
        if (args_token(p2, 32)) {
            print_dollar(msg_bad);
            return 1;
        }
        if (token_is(p1, "40")) {
            set_video_mode(1);
        } else if (token_is(p1, "BW80")) {
            set_video_mode(2);
        } else {
            set_video_mode(3);
        }
        print_dollar(msg_ok);
        return 0;
    }

    if (toupper_ch(buf_get(p1, 0)) == 'C' && toupper_ch(buf_get(p1, 1)) == 'O'
        && toupper_ch(buf_get(p1, 2)) == 'M') {
        c = buf_get(p1, 3);
        if (c != '1' || buf_get(p1, 4) != 0) {
            print_dollar(msg_bad);
            return 1;
        }
        if (!args_token(p2, 32)) {
            print_dollar(msg_bad);
            return 1;
        }
        if (!mode_com1(p2)) {
            print_dollar(msg_bad);
            return 1;
        }
        return 0;
    }

    if (toupper_ch(buf_get(p1, 0)) == 'L' && toupper_ch(buf_get(p1, 1)) == 'P'
        && toupper_ch(buf_get(p1, 2)) == 'T') {
        c = buf_get(p1, 3);
        if (c != 0 && c != '1') {
            print_dollar(msg_bad);
            return 1;
        }
        /* MODE LPT1:=COM1 — redirection has no backing engine. */
        i = 0;
        while (buf_get(p1, i) != 0) {
            if (buf_get(p1, i) == '=') {
                print_dollar(msg_lpt_r);
                return 1;
            }
            i = i + 1;
        }
        if (args_token(p2, 32)) {
            if (buf_get(p2, 0) == '=' || (buf_get(p2, 0) == ':' && buf_get(p2, 1) == '=')) {
                print_dollar(msg_lpt_r);
                return 1;
            }
            i = 0;
            while (buf_get(p2, i) != 0) {
                if (buf_get(p2, i) == '=') {
                    print_dollar(msg_lpt_r);
                    return 1;
                }
                i = i + 1;
            }
        }
        print_dollar(msg_lpt);
        print_dollar(msg_ok);
        return 0;
    }

    if (toupper_ch(buf_get(p1, 0)) == 'C' && toupper_ch(buf_get(p1, 1)) == 'O'
        && toupper_ch(buf_get(p1, 2)) == 'N') {
        /* INT 10 AH=0F: AH=columns, AL=mode */
        asm("mov ah, 0x0F");
        asm("int 0x10");
        asm("mov al, ah");
        asm("xor ah, ah");
        asm("mov [con_cols], ax");
        reload_ds();
        print_dollar(msg_con);
        print_num(con_cols);
        print_dollar(msg_crlf);
        print_dollar(msg_ok);
        return 0;
    }

    print_dollar(msg_u);
    return 1;
}
