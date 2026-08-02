/* DEBUG.COM — debuggee arena with D/E/U/A/N/L/W/R/G/T/P/Q. */
#include "dos.h"

#define LINE_MAX 80
#define ARENA_PARAS 0x800

static char line[LINE_MAX];
static char path[64];
static char xfer[128];
static char msg_banner[8] = "DEBUG\r\n$";
static char msg_ok[12] = "DEBUG OK\r\n$";
static char msg_gok[14] = "DEBUG G OK\r\n$";
static char msg_prompt[3] = "-$";
static char msg_err[18] = "ERROR\r\n$";
static char msg_nomem[22] = "Out of memory\r\n$";
static char msg_crlf[3] = "\r\n$";
static char hexdig[17] = "0123456789ABCDEF";

static int dbg_seg;
static int dbg_paras;
static int default_addr;
static int load_len;
static int have_path;
static int did_dump;
static int did_g;
static int parse_hex_val;
static int go_mode;

static int reg_ax;
static int reg_bx;
static int reg_cx;
static int reg_dx;
static int reg_si;
static int reg_di;
static int reg_bp;
static int reg_sp;
static int reg_ip;
static int reg_fl;
static int saved_ss;
static int saved_sp;
static int saved_bp;
static int old_int3_off;
static int old_int3_seg;
static int old_int1_off;
static int old_int1_seg;

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

static int parse_hex(char *s)
{
    int i;
    int v;
    int d;
    int c;
    i = 0;
    v = 0;
    if (buf_get(s, 0) == 0) {
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
    while (buf_get(s, i) == ' ' || buf_get(s, i) == 9) {
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

static int mem_get(int addr)
{
    asm("push es");
    asm("mov es, [dbg_seg]");
    asm("mov bx, [bp+4]");
    asm("mov al, es:[bx]");
    asm("mov ah, 0");
    asm("pop es");
}

static void mem_set(int addr, int val)
{
    asm("push es");
    asm("mov es, [dbg_seg]");
    asm("mov bx, [bp+6]");
    asm("mov ax, [bp+4]");
    asm("mov es:[bx], al");
    asm("pop es");
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

static void cmd_dump(int addr, int len)
{
    int i;
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
        print_hex2(mem_get(addr + i));
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
        mem_set(addr, parse_hex_val & 255);
        addr = addr + 1;
        if (addr - 0x100 > load_len) {
            load_len = addr - 0x100;
        }
    }
    default_addr = addr;
}

static int disasm_one(int addr)
{
    int op;
    int mod;
    int off;
    op = mem_get(addr);
    print_hex4(addr);
    print_char(' ');
    print_hex2(op);
    print_char(' ');
    if (op == 0x90) {
        print_string("NOP\r\n");
        return 1;
    }
    if (op == 0xC3) {
        print_string("RET\r\n");
        return 1;
    }
    if (op == 0xCC) {
        print_string("INT3\r\n");
        return 1;
    }
    if (op == 0xCD) {
        print_hex2(mem_get(addr + 1));
        print_string(" INT ");
        print_hex2(mem_get(addr + 1));
        print_dollar(msg_crlf);
        return 2;
    }
    if (op == 0xEB) {
        off = mem_get(addr + 1);
        if (off >= 128) {
            off = off - 256;
        }
        print_hex2(mem_get(addr + 1));
        print_string(" JMP SHORT ");
        print_hex4(addr + 2 + off);
        print_dollar(msg_crlf);
        return 2;
    }
    if (op == 0xE8 || op == 0xE9) {
        mod = mem_get(addr + 1) | (mem_get(addr + 2) << 8);
        print_hex2(mem_get(addr + 1));
        print_char(' ');
        print_hex2(mem_get(addr + 2));
        if (op == 0xE8) {
            print_string(" CALL ");
        } else {
            print_string(" JMP ");
        }
        print_hex4(addr + 3 + mod);
        print_dollar(msg_crlf);
        return 3;
    }
    if (op >= 0xB0 && op <= 0xB7) {
        print_hex2(mem_get(addr + 1));
        print_string(" MOV r8,imm\r\n");
        return 2;
    }
    if (op >= 0xB8 && op <= 0xBF) {
        print_hex2(mem_get(addr + 1));
        print_char(' ');
        print_hex2(mem_get(addr + 2));
        print_string(" MOV r16,imm\r\n");
        return 3;
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
    if (streq_tok(tok, "INT3")) {
        mem_set(addr, 0xCC);
        default_addr = addr + 1;
        return;
    }
    if (streq_tok(tok, "DB")) {
        i = next_tok(line, i, t2, 16);
        if (!parse_hex(t2)) {
            print_dollar(msg_err);
            return;
        }
        mem_set(addr, parse_hex_val & 255);
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
        if (v == 3) {
            mem_set(addr, 0xCC);
            default_addr = addr + 1;
            return;
        }
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
    }
    print_dollar(msg_err);
}

static void cmd_n(char *rest)
{
    int i;
    char tok[64];
    next_tok(rest, 0, tok, 64);
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
    int left;
    int got;
    int off;
    if (!have_path) {
        print_dollar(msg_err);
        return;
    }
    h = dos_open(path, 0);
    if (h == -1) {
        print_dollar(msg_err);
        return;
    }
    off = 0x100;
    left = 0x7000;
    load_len = 0;
    while (left > 0) {
        n = 128;
        if (n > left) {
            n = left;
        }
        got = dos_read(h, xfer, n);
        if (got == 0 || got == -1) {
            break;
        }
        n = 0;
        while (n < got) {
            mem_set(off, buf_get(xfer, n));
            off = off + 1;
            n = n + 1;
        }
        load_len = load_len + got;
        left = left - got;
    }
    dos_close(h);
    default_addr = 0x100;
    reg_ip = 0x100;
    print_string("Loaded ");
    print_num(load_len);
    print_string(" bytes\r\n");
}

static void cmd_w(void)
{
    int h;
    int left;
    int off;
    int n;
    int chunk;
    if (!have_path) {
        print_dollar(msg_err);
        return;
    }
    if (load_len < 1) {
        load_len = 1;
    }
    h = dos_create(path, 0);
    if (h == -1) {
        print_dollar(msg_err);
        return;
    }
    off = 0x100;
    left = load_len;
    while (left > 0) {
        chunk = 128;
        if (chunk > left) {
            chunk = left;
        }
        n = 0;
        while (n < chunk) {
            buf_set(xfer, n, mem_get(off + n));
            n = n + 1;
        }
        if (dos_write(h, xfer, chunk) != chunk) {
            dos_close(h);
            print_dollar(msg_err);
            return;
        }
        off = off + chunk;
        left = left - chunk;
    }
    dos_close(h);
    print_string("Wrote ");
    print_num(load_len);
    print_dollar(msg_crlf);
}

static void print_regs(void)
{
    print_string("AX=");
    print_hex4(reg_ax);
    print_string(" BX=");
    print_hex4(reg_bx);
    print_string(" CX=");
    print_hex4(reg_cx);
    print_string(" DX=");
    print_hex4(reg_dx);
    print_dollar(msg_crlf);
    print_string("SI=");
    print_hex4(reg_si);
    print_string(" DI=");
    print_hex4(reg_di);
    print_string(" BP=");
    print_hex4(reg_bp);
    print_string(" SP=");
    print_hex4(reg_sp);
    print_dollar(msg_crlf);
    print_string("CS=");
    print_hex4(dbg_seg);
    print_string(" IP=");
    print_hex4(reg_ip);
    print_string(" FL=");
    print_hex4(reg_fl);
    print_dollar(msg_crlf);
}

static void cmd_r(char *rest)
{
    int i;
    char tok[8];
    char t2[8];
    int v;
    i = next_tok(rest, 0, tok, 8);
    if (buf_get(tok, 0) == 0) {
        print_regs();
        return;
    }
    i = next_tok(rest, i, t2, 8);
    if (!parse_hex(t2)) {
        print_dollar(msg_err);
        return;
    }
    v = parse_hex_val;
    if (streq_tok(tok, "AX")) {
        reg_ax = v;
    } else if (streq_tok(tok, "BX")) {
        reg_bx = v;
    } else if (streq_tok(tok, "CX")) {
        reg_cx = v;
    } else if (streq_tok(tok, "DX")) {
        reg_dx = v;
    } else if (streq_tok(tok, "SI")) {
        reg_si = v;
    } else if (streq_tok(tok, "DI")) {
        reg_di = v;
    } else if (streq_tok(tok, "BP")) {
        reg_bp = v;
    } else if (streq_tok(tok, "SP")) {
        reg_sp = v;
    } else if (streq_tok(tok, "IP")) {
        reg_ip = v;
    } else if (streq_tok(tok, "F") || streq_tok(tok, "FL")) {
        reg_fl = v;
    } else {
        print_dollar(msg_err);
    }
}

void after_break(void)
{
    reload_ds();
    asm("push es");
    asm("xor ax, ax");
    asm("mov es, ax");
    asm("mov ax, [old_int3_off]");
    asm("mov es:[0x0C], ax");
    asm("mov ax, [old_int3_seg]");
    asm("mov es:[0x0E], ax");
    asm("mov ax, [old_int1_off]");
    asm("mov es:[0x04], ax");
    asm("mov ax, [old_int1_seg]");
    asm("mov es:[0x06], ax");
    asm("pop es");
    print_regs();
    if (go_mode == 1) {
        print_dollar(msg_gok);
        did_g = 1;
    }
    go_mode = 0;
    disasm_one(reg_ip);
}

static void run_debuggee(int mode)
{
    go_mode = mode;
    if (mode == 2 || mode == 3) {
        reg_fl = reg_fl | 0x100;
    } else {
        reg_fl = reg_fl & 0xFEFF;
    }
    asm("mov [saved_ss], ss");
    asm("mov [saved_bp], bp");
    asm("lea ax, [Lresume]");
    asm("push ax");
    asm("mov [saved_sp], sp");
    asm("push es");
    asm("xor ax, ax");
    asm("mov es, ax");
    asm("mov ax, es:[0x0C]");
    asm("mov [old_int3_off], ax");
    asm("mov ax, es:[0x0E]");
    asm("mov [old_int3_seg], ax");
    asm("mov ax, es:[0x04]");
    asm("mov [old_int1_off], ax");
    asm("mov ax, es:[0x06]");
    asm("mov [old_int1_seg], ax");
    asm("lea ax, [Lbreak]");
    asm("mov es:[0x0C], ax");
    asm("mov es:[0x0E], cs");
    asm("mov es:[0x04], ax");
    asm("mov es:[0x06], cs");
    asm("pop es");
    /* Load iret frame and GPRs while DS still = DEBUG; switch DS last. */
    asm("cli");
    asm("mov ax, [dbg_seg]");
    asm("mov ss, ax");
    asm("mov sp, [reg_sp]");
    asm("mov bx, [reg_fl]");
    asm("push bx");
    asm("push ax");
    asm("mov bx, [reg_ip]");
    asm("push bx");
    asm("mov bx, ax");
    asm("mov ax, [reg_ax]");
    asm("mov cx, [reg_cx]");
    asm("mov dx, [reg_dx]");
    asm("mov si, [reg_si]");
    asm("mov di, [reg_di]");
    asm("mov bp, [reg_bp]");
    asm("push ax");
    asm("mov ax, [reg_bx]");
    asm("mov ds, bx");
    asm("mov es, bx");
    asm("mov bx, ax");
    asm("pop ax");
    asm("sti");
    asm("iret");
    asm("Lbreak:");
    asm("cli");
    asm("mov word ptr cs:[reg_ax], ax");
    asm("mov word ptr cs:[reg_bx], bx");
    asm("mov word ptr cs:[reg_cx], cx");
    asm("mov word ptr cs:[reg_dx], dx");
    asm("mov word ptr cs:[reg_si], si");
    asm("mov word ptr cs:[reg_di], di");
    asm("mov word ptr cs:[reg_bp], bp");
    asm("pop ax");
    asm("mov word ptr cs:[reg_ip], ax");
    asm("pop ax");
    asm("pop ax");
    asm("mov word ptr cs:[reg_fl], ax");
    asm("and word ptr cs:[reg_fl], 0xFEFF");
    asm("mov word ptr cs:[reg_sp], sp");
    asm("mov ax, cs");
    asm("mov ds, ax");
    asm("mov es, ax");
    asm("mov ax, word ptr cs:[saved_ss]");
    asm("mov ss, ax");
    asm("mov sp, word ptr cs:[saved_sp]");
    asm("mov bp, word ptr cs:[saved_bp]");
    asm("sti");
    asm("call after_break");
    asm("ret");
    asm("Lresume:");
    reload_ds();
}

static void cmd_g(char *rest)
{
    int i;
    char tok[16];
    char t2[16];
    int j;
    i = next_tok(rest, 0, tok, 16);
    if (buf_get(tok, 0) != 0) {
        if (buf_get(tok, 0) == '=') {
            j = 0;
            while (buf_get(tok, j + 1) != 0) {
                buf_set(t2, j, buf_get(tok, j + 1));
                j = j + 1;
            }
            buf_set(t2, j, 0);
            if (!parse_hex(t2)) {
                print_dollar(msg_err);
                return;
            }
            reg_ip = parse_hex_val;
        } else if (parse_hex(tok)) {
            reg_ip = parse_hex_val;
        } else {
            print_dollar(msg_err);
            return;
        }
    }
    run_debuggee(1);
}

static void cmd_t(void)
{
    run_debuggee(2);
}

static int insn_len(int addr)
{
    int op;
    op = mem_get(addr);
    if (op == 0xCD || op == 0xEB) {
        return 2;
    }
    if (op == 0xE8 || op == 0xE9) {
        return 3;
    }
    if (op >= 0xB0 && op <= 0xB7) {
        return 2;
    }
    if (op >= 0xB8 && op <= 0xBF) {
        return 3;
    }
    return 1;
}

static void cmd_p(void)
{
    int op;
    int len;
    int saved;
    int at;
    op = mem_get(reg_ip);
    if (op == 0xE8 || op == 0xCD || op == 0x9A) {
        len = insn_len(reg_ip);
        at = reg_ip + len;
        saved = mem_get(at);
        mem_set(at, 0xCC);
        run_debuggee(1);
        if (mem_get(reg_ip) == 0xCC) {
            mem_set(reg_ip, saved);
        } else {
            mem_set(at, saved);
        }
        return;
    }
    cmd_t();
}

static int init_arena(void)
{
    int p;
    p = ARENA_PARAS;
    while (p >= 0x200) {
        dbg_seg = dos_alloc(p);
        if (dbg_seg != 0) {
            dbg_paras = p;
            return 1;
        }
        p = p - 0x100;
    }
    return 0;
}

int main(void)
{
    int i;
    int cmd;
    char tok[16];
    int addr;
    int len;
    char rest[LINE_MAX];
    int j;

    if (!init_arena()) {
        print_dollar(msg_nomem);
        return 1;
    }
    default_addr = 0x100;
    load_len = 0;
    have_path = 0;
    did_dump = 0;
    did_g = 0;
    reg_ax = 0;
    reg_bx = 0;
    reg_cx = 0;
    reg_dx = 0;
    reg_si = 0;
    reg_di = 0;
    reg_bp = 0;
    reg_sp = 0xFFFE;
    reg_ip = 0x100;
    reg_fl = 0x0202;
    go_mode = 0;

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
        j = 0;
        while (1) {
            buf_set(rest, j, buf_get(line, i + j));
            if (buf_get(line, i + j) == 0) {
                break;
            }
            j = j + 1;
        }
        if (cmd == 'Q') {
            break;
        }
        if (cmd == 'R') {
            cmd_r(rest);
            continue;
        }
        if (cmd == 'G') {
            cmd_g(rest);
            continue;
        }
        if (cmd == 'T') {
            cmd_t();
            continue;
        }
        if (cmd == 'P') {
            cmd_p();
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
            j = 0;
            while (buf_get(rest, i + j) != 0) {
                buf_set(line, j, buf_get(rest, i + j));
                j = j + 1;
            }
            buf_set(line, j, 0);
            cmd_enter(addr, line);
            continue;
        }
        if (cmd == 'U') {
            addr = default_addr;
            i = next_tok(rest, 0, tok, 16);
            if (buf_get(tok, 0) != 0) {
                if (!parse_hex(tok)) {
                    print_dollar(msg_err);
                    continue;
                }
                addr = parse_hex_val;
            }
            cmd_u(addr, 8);
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

    if (did_dump || did_g) {
        print_dollar(msg_ok);
    }
    dos_free(dbg_seg);
    return 0;
}
