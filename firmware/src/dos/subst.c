/* SUBST.COM — map a drive letter to a path (rmDOS INT 2Fh AX=12E0/E1). */
#include "dos.h"

static char arg1[80];
static char arg2[80];
static char msg_ok[14] = "SUBST OK\r\n$";
static char msg_err[18] = "SUBST failed\r\n$";
static char msg_usage[48] = "SUBST [d: path | d: /D]\r\n$";
static char msg_list[6] = ":\r\n$";

int is_drive(char *s)
{
    int c;
    c = buf_get(s, 0);
    if (c >= 'a' && c <= 'z') c = c - 32;
    if (c < 'A' || c > 'Z') return 0;
    if (buf_get(s, 1) != ':') return 0;
    return 1;
}

int drive_idx(char *s)
{
    int c;
    c = buf_get(s, 0);
    if (c >= 'a' && c <= 'z') c = c - 32;
    return c - 'A';
}

int subst_set(int letter, int real, char *path)
{
    asm("mov bl, [bp+4]");
    asm("mov bh, [bp+6]");
    asm("mov dx, [bp+8]");
    asm("mov ax, 0x12E0");
    asm("int 0x2F");
    asm("mov ah, 0");
}

int subst_del(int letter)
{
    asm("mov bl, [bp+4]");
    asm("mov ax, 0x12E1");
    asm("int 0x2F");
    asm("mov ah, 0");
}

int is_slash_d(char *s)
{
    int c;
    if (buf_get(s, 0) != '/') return 0;
    c = buf_get(s, 1);
    if (c >= 'a' && c <= 'z') c = c - 32;
    if (c != 'D') return 0;
    return 1;
}

int main(void)
{
    int d;
    int real;
    int r;

    args_init();
    if (!args_token(arg1, 80)) {
        print_dollar(msg_usage);
        return 0;
    }
    if (!is_drive(arg1)) {
        print_dollar(msg_usage);
        return 1;
    }
    d = drive_idx(arg1);
    if (!args_token(arg2, 80)) {
        print_dollar(msg_usage);
        return 1;
    }
    if (is_slash_d(arg2)) {
        r = subst_del(d);
        if (r != 0) {
            print_dollar(msg_err);
            return 1;
        }
        print_dollar(msg_ok);
        return 0;
    }
    if (!is_drive(arg2)) {
        print_dollar(msg_usage);
        return 1;
    }
    real = drive_idx(arg2);
    /* path may continue after "A:" in arg2, or next token */
    if (buf_get(arg2, 2) != 0) {
        r = subst_set(d, real, buf_addr(arg2, 2));
    } else if (args_token(arg1, 80)) {
        r = subst_set(d, real, arg1);
    } else {
        r = subst_set(d, real, "");
    }
    if (r != 0) {
        print_dollar(msg_err);
        return 1;
    }
    print_dollar(msg_ok);
    return 0;
}
