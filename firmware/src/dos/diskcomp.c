/* DISKCOMP.COM — compare floppies via INT 13h AH=02. */
#include "dos.h"

#define MAX_SPT 18

static char sec_a[512] = { 0 };
static char sec_b[512] = { 0 };
static char tok[16];
static char msg_u[36] = "DISKCOMP [d:] [d:] [/Y]\r\n$";
static char msg_bad[28] = "DISKCOMP: bad drive\r\n$";
static char msg_geo[34] = "DISKCOMP: geometry mismatch\r\n$";
static char msg_spt[30] = "DISKCOMP: SPT too large\r\n$";
static char msg_io[24] = "DISKCOMP: I/O error\r\n$";
static char msg_ins_a[40] = "Insert diskette 1, press a key\r\n$";
static char msg_ins_b[40] = "Insert diskette 2, press a key\r\n$";
static char msg_cmp[18] = "Comparing...\r\n$";
static char msg_ok[14] = "Compare OK\r\n$";
static char msg_mis[22] = "Compare error at $";
static char msg_crlf[3] = "\r\n$";

static int auto_yes;
static int drv_a;
static int drv_b;
static int geo_cyl;
static int geo_heads;
static int geo_spt;
static int i13_ax;
static int i13_cx;
static int i13_dx;
static int i13_bx;
static int i13_es;
static int i13_ok;
static int i13_tmp_spt;
static int i13_tmp_heads;
static int i13_tmp_cyl;
static int mis_cyl;
static int mis_head;
static int mis_sec;

static int parse_drive(char *s)
{
    int c;
    c = toupper_ch(buf_get(s, 0));
    if (c < 'A' || c > 'B') {
        return -1;
    }
    if (buf_get(s, 1) != 0 && buf_get(s, 1) != ':') {
        return -1;
    }
    return c - 'A';
}

static void i13_params(int dl)
{
    i13_dx = dl & 0xFF;
    asm("mov ah, 0x08");
    asm("mov dl, byte ptr [i13_dx]");
    asm("xor bx, bx");
    asm("int 0x13");
    asm("jc Li13p_fail");
    asm("mov al, cl");
    asm("xor ah, ah");
    asm("and ax, 0x3F");
    asm("mov [i13_tmp_spt], ax");
    asm("mov al, dh");
    asm("xor ah, ah");
    asm("inc ax");
    asm("mov [i13_tmp_heads], ax");
    asm("mov al, ch");
    asm("mov ah, cl");
    asm("mov cl, 6");
    asm("shr ah, cl");
    asm("inc ax");
    asm("mov [i13_tmp_cyl], ax");
    asm("mov word ptr [i13_ok], 1");
    asm("jmp Li13p_done");
    asm("Li13p_fail:");
    asm("mov word ptr [i13_ok], 0");
    asm("Li13p_done:");
    reload_ds();
    if (i13_ok) {
        geo_spt = i13_tmp_spt;
        geo_heads = i13_tmp_heads;
        geo_cyl = i13_tmp_cyl;
    }
}

static int i13_read(int dl, int cyl, int head, int sector_num, char *buf)
{
    i13_cx = ((cyl & 0xFF) << 8) | ((((cyl >> 8) & 3) << 6) | (sector_num & 0x3F));
    i13_dx = ((head & 0xFF) << 8) | (dl & 0xFF);
    i13_bx = buf_addr(buf, 0);
    asm("mov ax, cs");
    asm("mov [i13_es], ax");
    i13_ax = 0x0201;
    asm("push es");
    asm("push ds");
    asm("mov ax, [i13_es]");
    asm("mov es, ax");
    asm("mov ax, [i13_ax]");
    asm("mov bx, [i13_bx]");
    asm("mov cx, [i13_cx]");
    asm("mov dx, [i13_dx]");
    asm("int 0x13");
    asm("pop ds");
    asm("pop es");
    asm("jc Li13r_fail");
    asm("mov word ptr [i13_ok], 1");
    asm("jmp Li13r_done");
    asm("Li13r_fail:");
    asm("mov word ptr [i13_ok], 0");
    asm("Li13r_done:");
    reload_ds();
    return i13_ok;
}

static void prompt_key(char *msg)
{
    if (auto_yes) {
        return;
    }
    reload_ds();
    print_dollar(msg);
    read_key();
}

static int same_geo(int dl)
{
    int c;
    int h;
    int s;
    c = geo_cyl;
    h = geo_heads;
    s = geo_spt;
    i13_params(dl);
    if (!i13_ok) {
        return 0;
    }
    if (geo_cyl != c || geo_heads != h || geo_spt != s) {
        return 0;
    }
    return 1;
}

static int secs_eq(void)
{
    int i;
    i = 0;
    while (i < 512) {
        if (buf_get(sec_a, i) != buf_get(sec_b, i)) {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

int main(void)
{
    int have_a;
    int have_b;
    int cyl;
    int head;
    int sec;

    auto_yes = 0;
    drv_a = 0;
    drv_b = 1;
    have_a = 0;
    have_b = 0;

    args_init();
    while (args_skip()) {
        if (!args_token(tok, 16)) {
            break;
        }
        if (buf_get(tok, 0) == '/' || buf_get(tok, 0) == '-') {
            if (toupper_ch(buf_get(tok, 1)) == 'Y') {
                auto_yes = 1;
            }
        } else if (!have_a) {
            drv_a = parse_drive(tok);
            have_a = 1;
        } else if (!have_b) {
            drv_b = parse_drive(tok);
            have_b = 1;
        }
    }

    if (!have_a) {
        print_dollar(msg_u);
        return 1;
    }
    if (drv_a < 0 || (have_b && drv_b < 0)) {
        print_dollar(msg_bad);
        return 1;
    }
    if (!have_b) {
        drv_b = drv_a == 0 ? 1 : 0;
    }

    prompt_key(msg_ins_a);
    i13_params(drv_a);
    if (!i13_ok || geo_spt < 1 || geo_heads < 1 || geo_cyl < 1) {
        print_dollar(msg_io);
        return 1;
    }
    if (geo_spt > MAX_SPT) {
        print_dollar(msg_spt);
        return 1;
    }
    if (drv_a != drv_b) {
        if (!same_geo(drv_b)) {
            print_dollar(msg_geo);
            return 1;
        }
    }

    reload_ds();
    print_dollar(msg_cmp);
    cyl = 0;
    while (cyl < geo_cyl) {
        head = 0;
        while (head < geo_heads) {
            if (drv_a == drv_b) {
                prompt_key(msg_ins_a);
            }
            sec = 1;
            while (sec <= geo_spt) {
                if (!i13_read(drv_a, cyl, head, sec, sec_a)) {
                    reload_ds();
                    print_dollar(msg_io);
                    return 1;
                }
                if (drv_a == drv_b) {
                    prompt_key(msg_ins_b);
                }
                if (!i13_read(drv_b, cyl, head, sec, sec_b)) {
                    reload_ds();
                    print_dollar(msg_io);
                    return 1;
                }
                if (!secs_eq()) {
                    mis_cyl = cyl;
                    mis_head = head;
                    mis_sec = sec;
                    reload_ds();
                    print_dollar(msg_mis);
                    print_num(mis_cyl);
                    print_char('/');
                    print_num(mis_head);
                    print_char('/');
                    print_num(mis_sec);
                    print_dollar(msg_crlf);
                    return 1;
                }
                if (drv_a == drv_b) {
                    prompt_key(msg_ins_a);
                }
                sec = sec + 1;
            }
            head = head + 1;
        }
        reload_ds();
        print_char('.');
        cyl = cyl + 1;
    }

    reload_ds();
    print_dollar(msg_crlf);
    print_dollar(msg_ok);
    return 0;
}
