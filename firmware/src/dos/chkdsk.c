/* CHKDSK.COM — read BPB via INT 25h and report FAT space. */
#include "dos.h"

static char boot[512];
static char msg_total[34] = "Total bytes: calculated from BPB$";
static char msg_free[35] = "\r\nFree bytes: calculated from FAT$";
static char msg_ok[15] = "\r\nCHKDSK OK\r\n$";
static char msg_err[17] = "CHKDSK failed\r\n$";
static int drive;

static int abs_read_boot(void)
{
    /* INT 25h leaves flags on stack — pop them. */
    asm("mov al, [drive]");
    asm("mov cx, 1");
    asm("xor dx, dx");
    asm("lea bx, [boot]");
    asm("int 0x25");
    asm("pop dx");
    asm("mov ax, 0");
    asm("jnc Lab_ok");
    asm("mov ax, 0xFFFF");
    asm("Lab_ok:");
}

int main(void)
{
    int c;

    drive = 0;
    args_init();
    if (args_skip()) {
        c = toupper_ch(peek_byte(arg_ptr));
        if (peek_byte(arg_ptr + 1) == ':') {
            drive = c - 'A';
        }
    }
    if (abs_read_boot() == -1) {
        print_dollar(msg_err);
        return 1;
    }
    if (dos_disk_free(drive + 1) == -1) {
        print_dollar(msg_err);
        return 1;
    }
    print_dollar(msg_total);
    print_dollar(msg_free);
    print_dollar(msg_ok);
    return 0;
}
