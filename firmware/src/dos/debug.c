/* DEBUG.COM — classic-lite hex dump / enter / unassemble / assemble. */
#include "dos.h"

#define LOAD_MAX 8192
#define LINE_MAX 80

static char loadbuf[LOAD_MAX] = { 0 };
static char line[LINE_MAX];
static char path[64];
static char msg_banner[8] = "DEBUG\r\n$";
static char msg_ok[12] = "DEBUG OK\r\n$";
static char msg_prompt[3] = "-$";
static char msg_g[22] = "G not supported\r\n$";
static char msg_err[18] = "ERROR\r\n$";
static char msg_crlf[3] = "\r\n$";
static char hexdig[17] = "0123456789ABCDEF";

static int default_addr;
static int load_len;
static int have_path;
static int did_dump;
static int load_base; /* CS offset of loadbuf */

static int hex_val(int c)
{
    c = toupper_ch(c);
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static void print_hex2(int v)
{
    print_char(buf_get(hexdig, (v >> 4) & 15));
    print_char(buf_get(hexdig, v & 15));
}

static void print_hex4(int v)
{
    print_hex2((v >> 8) & 255);
    print_hex2(v & 255);
}

static int mem_get(int addr)
{
    asm("mov bx, [bp+4]");
    asm("mov al, cs:[bx]");
    asm("mov ah, 0");
}

static void mem_set(int addr, int val)
{
    asm("mov bx, [bp+6]");
    asm("mov ax, [bp+4]");
    asm("mov cs:[bx], al");
}

static int parse_hex_val;

static int parse_hex(char *s)
{
    int i;
    int v;
    int d;
    int c;
    i = 0;
    v = 0;
    c = buf_get(s, 0);
    if (c == 0) {
        return 0;
    }
    while (1) {
        c = buf_get(s, i);
        if (c == 0 || c == ' ' || c == 9) {
            break;
        }
        d = hex_val(c);
        if (d < 0) {
            return 0;
        }
        v = (v << 4) + d;
        i = i + 1;
    }
    parse_hex_val = v & 0xFFFF;
    return 1;
}

static int skip_tok(char *s, int i)
{
    int c;
    while (1) {
        c = buf_get(s, i);
        if (c != ' ' && c != 9) {
            break;
        }
        i = i + 1;
    }
    return i;
}

static int next_tok(char *s, int i, char *out, int max)
{
    int n;
    int c;
    i = skip_tok(s, i);
    n = 0;
    while (1) {
        c = buf_get(s, i);
        if (c == 0 || c == ' ' || c == 9) {
            break;
        }
        if (n < max - 1) {
            buf_set(out, n, c);
            n = n + 1;
        }
        i = i + 1;
    }
    buf_set(out, n, 0);
    return i;
}

static int read_line(void)
{
    int i;
    int n;
    int c;
    char one[2];
    i = 0;
    while (i < LINE_MAX - 1) {
        n = dos_read(0, one, 1);
        if (n == 0 || n == -1) {
            if (i == 0) {
                return 0;
            }
            break;
        }
        c = buf_get(one, 0);
        if (c == 13) {
            dos_read(0, one, 1);
            break;
        }
        if (c == 10) {
            break;
        }
        buf_set(line, i, c);
        i = i + 1;
    }
    buf_set(line, i, 0);
    return 1;
}

static void cmd_dump(int addr, int len)
{
    int i;
    int b;
    if (len < 1) {
        len = 128;
    }
    i = 0;
    while (i < len) {
        if ((i & 15) == 0) {
            if (i != 0) {
                print_dollar(msg_crlf);
            }
            print_hex4(addr + i);
            print_char(' ');
        }
        b = mem_get(addr + i);
        print_hex2(b);
        print_char(' ');
        i = i + 1;
    }
    print_dollar(msg_crlf);
    default_addr = addr + len;
    did_dump = 1;
}

static void cmd_enter(int addr, char *rest)
{
    int i;
    char tok[8];
    int v;
    i = 0;
    while (1) {
        i = next_tok(rest, i, tok, 8);
        if (buf_get(tok, 0) == 0) {
            break;
        }
        if (!parse_hex(tok)) {
            print_dollar(msg_err);
            return;
        }
        v = parse_hex_val;
        mem_set(addr, v & 255);
        addr = addr + 1;
    }
    default_addr = addr;
}

static int disasm_one(int addr)
{
    int op;
    int mod;
    op = mem_get(addr);
    print_hex4(addr);
    print_char(' ');
    print_hex2(op);
    print_char(' ');
    if (op == 0x90) {
        print_string("NOP");
        print_dollar(msg_crlf);
        return 1;
    }
    if (op == 0xC3) {
        print_string("RET");
        print_dollar(msg_crlf);
        return 1;
    }
    if (op == 0xCD) {
        print_hex2(mem_get(addr + 1));
        print_char(' ');
        print_string("INT ");
        print_hex2(mem_get(addr + 1));
        print_dollar(msg_crlf);
        return 2;
    }
    if (op == 0xEB) {
        print_hex2(mem_get(addr + 1));
        print_char(' ');
        print_string("JMP SHORT ");
        {
            int off;
            off = mem_get(addr + 1);
            if (off >= 128) {
                off = off - 256;
            }
            print_hex4(addr + 2 + off);
        }
        print_dollar(msg_crlf);
        return 2;
    }
    if (op == 0xE9) {
        mod = mem_get(addr + 1) | (mem_get(addr + 2) << 8);
        print_hex2(mem_get(addr + 1));
        print_char(' ');
        print_hex2(mem_get(addr + 2));
        print_char(' ');
        print_string("JMP ");
        print_hex4(addr + 3 + mod);
        print_dollar(msg_crlf);
        return 3;
    }
    if (op == 0xE8) {
        mod = mem_get(addr + 1) | (mem_get(addr + 2) << 8);
        print_hex2(mem_get(addr + 1));
        print_char(' ');
        print_hex2(mem_get(addr + 2));
        print_char(' ');
        print_string("CALL ");
        print_hex4(addr + 3 + mod);
        print_dollar(msg_crlf);
        return 3;
    }
    if (op == 0xB0 || op == 0xB1 || op == 0xB2 || op == 0xB3 ||
        op == 0xB4 || op == 0xB5 || op == 0xB6 || op == 0xB7) {
        print_hex2(mem_get(addr + 1));
        print_char(' ');
        print_string("MOV r8,");
        print_hex2(mem_get(addr + 1));
        print_dollar(msg_crlf);
        return 2;
    }
    if (op == 0xB8 || op == 0xB9 || op == 0xBA || op == 0xBB ||
        op == 0xBC || op == 0xBD || op == 0xBE || op == 0xBF) {
        print_hex2(mem_get(addr + 1));
        print_char(' ');
        print_hex2(mem_get(addr + 2));
        print_char(' ');
        print_string("MOV r16,");
        print_hex4(mem_get(addr + 1) | (mem_get(addr + 2) << 8));
        print_dollar(msg_crlf);
        return 3;
    }
    if (op >= 0x50 && op <= 0x57) {
        print_string("PUSH r16");
        print_dollar(msg_crlf);
        return 1;
    }
    if (op >= 0x58 && op <= 0x5F) {
        print_string("POP r16");
        print_dollar(msg_crlf);
        return 1;
    }
    print_string("DB ");
    print_hex2(op);
    print_dollar(msg_crlf);
    return 1;
}

static void cmd_u(int addr, int count)
{
    int n;
    int step;
    if (count < 1) {
        count = 8;
    }
    n = 0;
    while (n < count) {
        step = disasm_one(addr);
        addr = addr + step;
        n = n + 1;
    }
    default_addr = addr;
}

static int streq_tok(char *a, char *b)
{
    int i;
    int ca;
    int cb;
    i = 0;
    while (1) {
        ca = toupper_ch(buf_get(a, i));
        cb = toupper_ch(buf_get(b, i));
        if (ca != cb) {
            return 0;
        }
        if (ca == 0) {
            return 1;
        }
        i = i + 1;
    }
}

static void cmd_a(int addr)
{
    int i;
    char tok[16];
    char t2[16];
    int v;
    int v2;
    print_hex4(addr);
    print_char(':');
    print_char(' ');
    if (!read_line()) {
        return;
    }
    i = next_tok(line, 0, tok, 16);
    if (buf_get(tok, 0) == 0) {
        return;
    }
    if (streq_tok(tok, "NOP")) {
        mem_set(addr, 0x90);
        default_addr = addr + 1;
        return;
    }
    if (streq_tok(tok, "RET")) {
        mem_set(addr, 0xC3);
        default_addr = addr + 1;
        return;
    }
    if (streq_tok(tok, "DB")) {
        i = next_tok(line, i, t2, 16);
        if (!parse_hex(t2)) {
            print_dollar(msg_err);
            return;
        }
        v = parse_hex_val;
        mem_set(addr, v & 255);
        default_addr = addr + 1;
        return;
    }
    if (streq_tok(tok, "INT")) {
        i = next_tok(line, i, t2, 16);
        if (!parse_hex(t2)) {
            print_dollar(msg_err);
            return;
        }
        v = parse_hex_val;
        mem_set(addr, 0xCD);
        mem_set(addr + 1, v & 255);
        default_addr = addr + 2;
        return;
    }
    if (streq_tok(tok, "JMP")) {
        i = next_tok(line, i, t2, 16);
        if (streq_tok(t2, "SHORT")) {
            i = next_tok(line, i, t2, 16);
            if (!parse_hex(t2)) {
                print_dollar(msg_err);
                return;
            }
            v = parse_hex_val;
            v2 = (v - (addr + 2)) & 255;
            mem_set(addr, 0xEB);
            mem_set(addr + 1, v2);
            default_addr = addr + 2;
            return;
        }
        print_dollar(msg_err);
        return;
    }
    print_dollar(msg_err);
}

static void cmd_n(char *rest)
{
    int i;
    char tok[64];
    i = next_tok(rest, 0, tok, 64);
    if (buf_get(tok, 0) == 0) {
        print_dollar(msg_err);
        return;
    }
    i = 0;
    while (1) {
        buf_set(path, i, buf_get(tok, i));
        if (buf_get(tok, i) == 0) {
            break;
        }
        i = i + 1;
    }
    have_path = 1;
}

static void cmd_l(void)
{
    int h;
    int n;
    if (!have_path) {
        print_dollar(msg_err);
        return;
    }
    h = dos_open(path, 0);
    if (h == -1) {
        print_dollar(msg_err);
        return;
    }
    n = dos_read(h, loadbuf, LOAD_MAX);
    dos_close(h);
    if (n < 0) {
        n = 0;
    }
    load_len = n;
    default_addr = load_base;
    print_string("Loaded ");
    print_num(load_len);
    print_string(" bytes at ");
    print_hex4(load_base);
    print_dollar(msg_crlf);
}

static void cmd_w(void)
{
    int h;
    int n;
    if (!have_path || load_len < 1) {
        print_dollar(msg_err);
        return;
    }
    h = dos_create(path, 0);
    if (h == -1) {
        print_dollar(msg_err);
        return;
    }
    n = dos_write(h, loadbuf, load_len);
    dos_close(h);
    if (n != load_len) {
        print_dollar(msg_err);
        return;
    }
    print_string("Wrote ");
    print_num(load_len);
    print_dollar(msg_crlf);
}

int main(void)
{
    int i;
    int cmd;
    char tok[16];
    int addr;
    int len;
    char rest[LINE_MAX];

    load_base = buf_addr(loadbuf, 0);
    default_addr = 0x100;
    load_len = 0;
    have_path = 0;
    did_dump = 0;

    print_dollar(msg_banner);

    while (1) {
        print_dollar(msg_prompt);
        if (!read_line()) {
            break;
        }
        i = next_tok(line, 0, tok, 16);
        if (buf_get(tok, 0) == 0) {
            continue;
        }
        cmd = toupper_ch(buf_get(tok, 0));
        /* copy remainder */
        {
            int j;
            j = 0;
            while (1) {
                buf_set(rest, j, buf_get(line, i + j));
                if (buf_get(line, i + j) == 0) {
                    break;
                }
                j = j + 1;
            }
        }
        if (cmd == 'Q') {
            break;
        }
        if (cmd == 'G') {
            print_dollar(msg_g);
            continue;
        }
        if (cmd == 'D') {
            addr = default_addr;
            len = 128;
            i = next_tok(rest, 0, tok, 16);
            if (buf_get(tok, 0) != 0) {
                if (!parse_hex(tok)) {
                    print_dollar(msg_err);
                    continue;
                }
                addr = parse_hex_val;
                i = next_tok(rest, i, tok, 16);
                if (toupper_ch(buf_get(tok, 0)) == 'L') {
                    i = next_tok(rest, i, tok, 16);
                    if (!parse_hex(tok)) {
                        print_dollar(msg_err);
                        continue;
                    }
                    len = parse_hex_val;
                } else if (buf_get(tok, 0) != 0) {
                    if (!parse_hex(tok)) {
                        print_dollar(msg_err);
                        continue;
                    }
                    len = parse_hex_val - addr + 1;
                }
            }
            cmd_dump(addr, len);
            continue;
        }
        if (cmd == 'E') {
            i = next_tok(rest, 0, tok, 16);
            if (!parse_hex(tok)) {
                print_dollar(msg_err);
                continue;
            }
            addr = parse_hex_val;
            /* pass rest after address */
            {
                int j;
                j = 0;
                while (buf_get(rest, i + j) != 0) {
                    buf_set(line, j, buf_get(rest, i + j));
                    j = j + 1;
                }
                buf_set(line, j, 0);
            }
            cmd_enter(addr, line);
            continue;
        }
        if (cmd == 'U') {
            addr = default_addr;
            len = 8;
            i = next_tok(rest, 0, tok, 16);
            if (buf_get(tok, 0) != 0) {
                if (!parse_hex(tok)) {
                    print_dollar(msg_err);
                    continue;
                }
                addr = parse_hex_val;
            }
            cmd_u(addr, len);
            continue;
        }
        if (cmd == 'A') {
            addr = default_addr;
            i = next_tok(rest, 0, tok, 16);
            if (buf_get(tok, 0) != 0) {
                if (!parse_hex(tok)) {
                    print_dollar(msg_err);
                    continue;
                }
                addr = parse_hex_val;
            }
            cmd_a(addr);
            continue;
        }
        if (cmd == 'N') {
            cmd_n(rest);
            continue;
        }
        if (cmd == 'L') {
            cmd_l();
            continue;
        }
        if (cmd == 'W') {
            cmd_w();
            continue;
        }
        print_dollar(msg_err);
    }

    if (did_dump) {
        print_dollar(msg_ok);
    }
    return 0;
}
