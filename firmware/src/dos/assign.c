/* ASSIGN.COM — map drive letter to another drive (via SUBST INT 2Fh). */
#include "dos.h"

static char arg1[32];
static char arg2[32];
static char msg_ok[14] = "ASSIGN OK\r\n$";
static char msg_err[18] = "ASSIGN failed\r\n$";
static char msg_usage[40] = "ASSIGN [d:=d:] | ASSIGN\r\n$";
static char root[4];

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

int assign_set(int letter, int real)
{
    /* Small-C: last arg at [bp+4], first at [bp+6]. */
    asm("mov bh, [bp+4]");
    asm("mov bl, [bp+6]");
    asm("lea dx, [root]");
    asm("mov ax, 0x12E0");
    asm("int 0x2F");
    asm("mov ah, 0");
}

int assign_del(int letter)
{
    asm("mov bl, [bp+4]");
    asm("mov ax, 0x12E1");
    asm("int 0x2F");
    asm("mov ah, 0");
}

void clear_all(void)
{
    int i;
    /* Leave A:/B:; clear C:.. (up to common LASTDRIVE). */
    i = 2;
    while (i < 26) {
        assign_del(i);
        i = i + 1;
    }
}

int main(void)
{
    int left;
    int right;
    int r;
    int i;
    int c;

    buf_set(root, 0, '\\');
    buf_set(root, 1, 0);
    args_init();
    if (!args_token(arg1, 32)) {
        clear_all();
        print_dollar(msg_ok);
        return 0;
    }
    if (buf_get(arg1, 0) == '/' || buf_get(arg1, 0) == '-') {
        c = toupper_ch(buf_get(arg1, 1));
        if (c == 'C') {
            clear_all();
            print_dollar(msg_ok);
            return 0;
        }
        print_dollar(msg_usage);
        return 1;
    }
    if (!is_drive(arg1)) {
        print_dollar(msg_usage);
        return 1;
    }
    left = drive_idx(arg1);
    i = 2;
    if (buf_get(arg1, i) == '=') {
        i = i + 1;
        if (buf_get(arg1, i) == 0) {
            if (!args_token(arg2, 32) || !is_drive(arg2)) {
                print_dollar(msg_usage);
                return 1;
            }
            right = drive_idx(arg2);
        } else {
            if (!is_drive(buf_addr(arg1, i))) {
                print_dollar(msg_usage);
                return 1;
            }
            right = drive_idx(buf_addr(arg1, i));
        }
    } else {
        if (!args_token(arg2, 32)) {
            print_dollar(msg_usage);
            return 1;
        }
        /* ASSIGN D: A:  or  ASSIGN D: =A: */
        if (buf_get(arg2, 0) == '=') {
            if (buf_get(arg2, 1) == 0) {
                if (!args_token(arg2, 32) || !is_drive(arg2)) {
                    print_dollar(msg_usage);
                    return 1;
                }
                right = drive_idx(arg2);
            } else if (!is_drive(buf_addr(arg2, 1))) {
                print_dollar(msg_usage);
                return 1;
            } else {
                right = drive_idx(buf_addr(arg2, 1));
            }
        } else if (!is_drive(arg2)) {
            print_dollar(msg_usage);
            return 1;
        } else {
            right = drive_idx(arg2);
        }
    }
    r = assign_set(left, right);
    if (r != 0) {
        print_dollar(msg_err);
        return 1;
    }
    print_dollar(msg_ok);
    return 0;
}
