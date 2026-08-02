/* LABEL.COM — show or set volume label (attr 08h). */
#include "dos.h"

static char all[4] = "*.*";
static char search[8];
static char label[12];
static char tok[12];
static char path[16];
static char dta[128];
static char msg_ok[20] = "Volume label set\r\n$";
static char msg_label[18] = "Volume label is $";
static char msg_none[23] = "Volume has no label\r\n$";
static char msg_err[16] = "LABEL failed\r\n$";
static char msg_u[32] = "LABEL [drive:] [label]\r\n$";
static char crlf[4] = "\r\n$";

static int is_drive(char *s)
{
    int c;

    c = toupper_ch(buf_get(s, 0));
    if (c < 'A' || c > 'Z') {
        return 0;
    }
    return buf_get(s, 1) == ':' && buf_get(s, 2) == 0;
}

static void copy_name(char *dst, char *src)
{
    int i;
    int c;

    i = 0;
    while (i < 11) {
        c = buf_get(src, i);
        buf_set(dst, i, c);
        if (c == 0) {
            return;
        }
        i = i + 1;
    }
    buf_set(dst, 11, 0);
}

static void make_path(char *out, int drive, char *name)
{
    int i;
    int c;

    i = 0;
    if (drive >= 0) {
        buf_set(out, i, drive + 'A');
        buf_set(out, i + 1, ':');
        buf_set(out, i + 2, '\\');
        i = 3;
    }
    while (i < 15) {
        c = buf_get(name, i - (drive >= 0 ? 3 : 0));
        if (c == 0) {
            break;
        }
        buf_set(out, i, c);
        i = i + 1;
    }
    buf_set(out, i, 0);
}

int main(void)
{
    int h;
    int drive;
    int have_label;

    drive = -1;
    have_label = 0;
    args_init();
    if (args_token(tok, 12)) {
        if (is_drive(tok)) {
            drive = toupper_ch(buf_get(tok, 0)) - 'A';
            if (args_token(label, 12)) {
                have_label = 1;
            }
        } else {
            copy_name(label, tok);
            have_label = 1;
        }
        if (args_token(tok, 12)) {
            print_dollar(msg_u);
            return 1;
        }
    }

    make_path(search, drive, all);
    if (!have_label) {
        dos_set_dta(dta);
        if (dos_find_first(search, 8) == -1) {
            print_dollar(msg_none);
            return 0;
        }
        print_dollar(msg_label);
        print_string(buf_addr(dta, 0x1E));
        print_dollar(crlf);
        return 0;
    }
    dos_set_dta(dta);
    if (dos_find_first(search, 8) == 0) {
        make_path(path, drive, buf_addr(dta, 0x1E));
        dos_delete(path);
    }
    make_path(path, drive, label);
    h = dos_create(path, 8);
    if (h == -1) {
        print_dollar(msg_err);
        return 1;
    }
    dos_close(h);
    print_dollar(msg_ok);
    return 0;
}
