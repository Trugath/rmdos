/* COPY.COM — COPY [/V][/A][/B] src dst. */
#include "dos.h"

static char src[64];
static char dst[64];
static char buf[512];
static char msg_ok[12] = "COPYV OK\r\n$";
static char msg_copied[10] = "copied\r\n$";
static char msg_err[15] = "COPY failed\r\n$";
static char msg_u[28] = "COPY [/V][/A][/B] src dst\r\n$";
static int opt_v;
static int opt_a;
static int opt_b;
static int verify_saved;

static void eat_switch(void)
{
    int c;
    arg_ptr = arg_ptr + 1;
    c = toupper_ch(peek_byte(arg_ptr));
    if (c == 'V') {
        opt_v = 1;
    } else if (c == 'A') {
        opt_a = 1;
        opt_b = 0;
    } else if (c == 'B') {
        opt_b = 1;
        opt_a = 0;
    }
    while (1) {
        c = peek_byte(arg_ptr);
        if (c == ' ' || c == 9 || c == 13 || c == 0) {
            break;
        }
        arg_ptr = arg_ptr + 1;
    }
}

int main(void)
{
    int hin;
    int hout;
    int n;
    int i;
    int c;
    int done;

    opt_v = 0;
    opt_a = 0;
    opt_b = 1;
    args_init();
    while (args_skip()) {
        c = peek_byte(arg_ptr);
        if (c == '/' || c == '-') {
            eat_switch();
        } else {
            break;
        }
    }
    if (!args_token(src, 64)) {
        print_dollar(msg_u);
        return 1;
    }
    while (args_skip()) {
        c = peek_byte(arg_ptr);
        if (c == '/' || c == '-') {
            eat_switch();
        } else {
            break;
        }
    }
    if (!args_token(dst, 64)) {
        print_dollar(msg_u);
        return 1;
    }

    if (opt_v) {
        asm("mov ah, 0x54");
        asm("int 0x21");
        asm("mov ah, 0");
        asm("mov [verify_saved], ax");
        asm("mov ax, 0x2E01");
        asm("int 0x21");
        reload_ds();
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
    done = 0;
    while (!done) {
        n = dos_read(hin, buf, 512);
        if (n == 0 || n == -1) {
            break;
        }
        if (opt_a) {
            i = 0;
            while (i < n) {
                if (buf_get(buf, i) == 26) {
                    n = i;
                    done = 1;
                    break;
                }
                i = i + 1;
            }
        }
        if (n > 0 && dos_write(hout, buf, n) == -1) {
            break;
        }
    }
    dos_close(hout);
    dos_close(hin);

    if (opt_v) {
        asm("mov al, byte ptr [verify_saved]");
        asm("mov ah, 0x2E");
        asm("int 0x21");
        reload_ds();
        print_dollar(msg_ok);
    } else {
        print_dollar(msg_copied);
    }
    return 0;
}
