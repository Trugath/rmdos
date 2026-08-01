/* DISKCOPY.COM — whole-floppy copy via INT 13h AH=02/03. */
#include "dos.h"

#define MAX_SPT 18

static char sector[512] = { 0 };
static char tok[16];
static char msg_u[36] = "DISKCOPY [d:] [d:] [/Y]\r\n$";
static char msg_bad[28] = "DISKCOPY: bad drive\r\n$";
static char msg_geo[34] = "DISKCOPY: geometry mismatch\r\n$";
static char msg_spt[30] = "DISKCOPY: SPT too large\r\n$";
static char msg_io[24] = "DISKCOPY: I/O error\r\n$";
static char msg_ins_src[42] = "Insert SOURCE diskette, press a key\r\n$";
static char msg_ins_dst[42] = "Insert TARGET diskette, press a key\r\n$";
static char msg_copy[18] = "Copying...\r\n$";
static char msg_ok[14] = "DISKCOPY OK\r\n$";
static char msg_crlf[3] = "\r\n$";

static int auto_yes;
static int src_dl;
static int dst_dl;
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

static int i13_rw(int write, int dl, int cyl, int head, int sector_num)
{
    /* Pack CX: CH=cyl lo, CL=sector | cyl hi bits */
    i13_cx = ((cyl & 0xFF) << 8) | ((((cyl >> 8) & 3) << 6) | (sector_num & 0x3F));
    i13_dx = ((head & 0xFF) << 8) | (dl & 0xFF);
    i13_bx = buf_addr(sector, 0);
    asm("mov ax, cs");
    asm("mov [i13_es], ax");
    if (write) {
        i13_ax = 0x0301;
    } else {
        i13_ax = 0x0201;
    }
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
    asm("jc Li13rw_fail");
    asm("mov word ptr [i13_ok], 1");
    asm("jmp Li13rw_done");
    asm("Li13rw_fail:");
    asm("mov word ptr [i13_ok], 0");
    asm("Li13rw_done:");
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

int main(void)
{
    int have_src;
    int have_dst;
    int cyl;
    int head;
    int sec;

    auto_yes = 0;
    src_dl = 0;
    dst_dl = 0;
    have_src = 0;
    have_dst = 0;

    args_init();
    while (args_skip()) {
        if (!args_token(tok, 16)) {
            break;
        }
        if (buf_get(tok, 0) == '/' || buf_get(tok, 0) == '-') {
            if (toupper_ch(buf_get(tok, 1)) == 'Y') {
                auto_yes = 1;
            }
        } else if (!have_src) {
            src_dl = parse_drive(tok);
            have_src = 1;
        } else if (!have_dst) {
            dst_dl = parse_drive(tok);
            have_dst = 1;
        }
    }

    if (!have_src) {
        print_dollar(msg_u);
        return 1;
    }
    if (src_dl < 0 || (have_dst && dst_dl < 0)) {
        print_dollar(msg_bad);
        return 1;
    }
    if (!have_dst) {
        dst_dl = src_dl;
    }

    prompt_key(msg_ins_src);
    i13_params(src_dl);
    if (!i13_ok || geo_spt < 1 || geo_heads < 1 || geo_cyl < 1) {
        print_dollar(msg_io);
        return 1;
    }
    if (geo_spt > MAX_SPT) {
        print_dollar(msg_spt);
        return 1;
    }
    if (src_dl != dst_dl) {
        if (!same_geo(dst_dl)) {
            print_dollar(msg_geo);
            return 1;
        }
    }

    reload_ds();
    print_dollar(msg_copy);
    cyl = 0;
    while (cyl < geo_cyl) {
        head = 0;
        while (head < geo_heads) {
            if (src_dl == dst_dl) {
                prompt_key(msg_ins_src);
            }
            sec = 1;
            while (sec <= geo_spt) {
                if (!i13_rw(0, src_dl, cyl, head, sec)) {
                    reload_ds();
                    print_dollar(msg_io);
                    return 1;
                }
                if (src_dl == dst_dl) {
                    prompt_key(msg_ins_dst);
                }
                if (!i13_rw(1, dst_dl, cyl, head, sec)) {
                    reload_ds();
                    print_dollar(msg_io);
                    return 1;
                }
                if (src_dl == dst_dl) {
                    prompt_key(msg_ins_src);
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
