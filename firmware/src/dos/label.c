/* LABEL.COM — show or set volume label (attr 08h). */
#include "dos.h"

static char all[4] = "*.*";
static char label[12];
static char dta[128];
static char msg_ok[20] = "Volume label set\r\n$";
static char msg_label[18] = "Volume label is $";
static char msg_none[23] = "Volume has no label\r\n$";
static char msg_err[16] = "LABEL failed\r\n$";
static char crlf[4] = "\r\n$";

int main(void)
{
    int h;

    args_init();
    if (!args_token(label, 12)) {
        dos_set_dta(dta);
        if (dos_find_first(all, 8) == -1) {
            print_dollar(msg_none);
            return 0;
        }
        print_dollar(msg_label);
        print_string(buf_addr(dta, 0x1E));
        print_dollar(crlf);
        return 0;
    }
    dos_set_dta(dta);
    if (dos_find_first(all, 8) == 0) {
        dos_delete(buf_addr(dta, 0x1E));
    }
    h = dos_create(label, 8);
    if (h == -1) {
        print_dollar(msg_err);
        return 1;
    }
    dos_close(h);
    print_dollar(msg_ok);
    return 0;
}
