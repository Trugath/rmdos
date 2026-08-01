/* DEL.COM — delete file named in PSP tail. */
#include "dos.h"

static char name[64];
static char msg_ok[11] = "deleted\r\n$";
static char msg_err[14] = "DEL failed\r\n$";
static char msg_u[12] = "DEL file\r\n$";

int main(void)
{
    args_init();
    if (!args_token(name, 64)) {
        print_dollar(msg_u);
        return 1;
    }
    if (dos_delete(name) == -1) {
        print_dollar(msg_err);
        return 1;
    }
    print_dollar(msg_ok);
    return 0;
}
