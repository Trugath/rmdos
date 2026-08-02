/*
 * COMMAND.COM for rmDOS, built with the in-tree Small-C compiler.
 * Keep this file within the intentionally small wcc language subset.
 */
#include "dos.h"

#define LINE_MAX 80
#define PATH_MAX 64
#define DTA_SIZE 128
#define BATCH_MAX 4

static char line[82] = { 0 };
static char cmd[82] = { 0 };
static char prog[80] = { 0 };
static char prog_base[64] = { 0 };
static char arg1[64] = { 0 };
static char arg2[64] = { 0 };
static char cwd[64] = { 0 };
static char pattern[64] = { 0 };
static char dta[DTA_SIZE] = { 0 };
static char copybuf[64] = { 0 };
static char pipe_rhs[82] = { 0 };
static char redir_name[64] = { 0 };
static char exec_tail[96] = { 0 };
static char exec_pb[14] = { 0 };
static char fcb1[16] = { 0 };
static char fcb2[16] = { 0 };
static char batch_names[128] = { 0 };
static char batch_args[160] = { 0 };
static char goto_name[64] = { 0 };
static char last_set[80] = { 0 };
static char prompt_fmt[80] = { '$', 'p', '$', 'g', 0 };
static char for_var[4] = { 0 };
static char for_body[82] = { 0 };
static char for_item[64] = { 0 };
static int cursor;
static int last_errorlevel;
static int batch_depth;
static int batch_handles[4] = { 0 };
static int goto_active;
static int redir_kind;
static int redir_handle;
static int saved_stdin;
static int saved_stdout;
static int dir_count;
static int dir_bytes;
static int verify_on;
static int break_on;
static int batch_argc;
static int batch_abort;

void dos_set_verify(int on)
{
    asm("mov al, [bp+4]");
    asm("mov ah, 0x2E");
    asm("int 0x21");
    asm("push cs");
    asm("pop ds");
}

void dos_set_break(int on)
{
    asm("mov dl, [bp+4]");
    asm("mov ax, 0x3301");
    asm("int 0x21");
    asm("push cs");
    asm("pop ds");
}

int dos_get_break(void)
{
    asm("mov ax, 0x3300");
    asm("int 0x21");
    asm("xor ah, ah");
    asm("mov al, dl");
    asm("push cs");
    asm("pop ds");
}
int dos_chdir(char *path)
{
    asm("mov dx, [bp+4]");
    asm("mov ah, 0x3B");
    asm("int 0x21");
    asm("mov ax, 0");
    asm("jnc Lcmd_chdir_ok");
    asm("mov ax, 0xFFFF");
    asm("Lcmd_chdir_ok:");
}

int dos_mkdir(char *path)
{
    asm("mov dx, [bp+4]");
    asm("mov ah, 0x39");
    asm("int 0x21");
    asm("mov ax, 0");
    asm("jnc Lcmd_mkdir_ok");
    asm("mov ax, 0xFFFF");
    asm("Lcmd_mkdir_ok:");
}

int dos_rmdir(char *path)
{
    asm("mov dx, [bp+4]");
    asm("mov ah, 0x3A");
    asm("int 0x21");
    asm("mov ax, 0");
    asm("jnc Lcmd_rmdir_ok");
    asm("mov ax, 0xFFFF");
    asm("Lcmd_rmdir_ok:");
}

int dos_dup(int handle)
{
    asm("mov bx, [bp+4]");
    asm("mov ah, 0x45");
    asm("int 0x21");
    asm("jnc Lcmd_dup_ok");
    asm("mov ax, 0xFFFF");
    asm("Lcmd_dup_ok:");
}

int dos_force_dup(int old_handle, int new_handle)
{
    asm("mov bx, [bp+6]");
    asm("mov cx, [bp+4]");
    asm("mov ah, 0x46");
    asm("int 0x21");
    asm("mov ax, 0");
    asm("jnc Lcmd_fdup_ok");
    asm("mov ax, 0xFFFF");
    asm("Lcmd_fdup_ok:");
}

int dos_seek_end(int handle)
{
    asm("mov bx, [bp+4]");
    asm("mov ax, 0x4202");
    asm("xor cx, cx");
    asm("xor dx, dx");
    asm("int 0x21");
    asm("jnc Lcmd_seek_ok");
    asm("mov ax, 0xFFFF");
    asm("Lcmd_seek_ok:");
}

int dos_current_drive(void)
{
    asm("mov ah, 0x19");
    asm("int 0x21");
    asm("mov ah, 0");
}

int dos_exit_code(void)
{
    asm("mov ah, 0x4D");
    asm("int 0x21");
    asm("mov ah, 0");
}

int dos_version(void)
{
    asm("mov ah, 0x30");
    asm("int 0x21");
}

static int g_year;
static int g_month;
static int g_day;
static int g_hour;
static int g_min;
static int g_sec;

void dos_get_date(void)
{
    asm("mov ah, 0x2A");
    asm("int 0x21");
    asm("mov [g_year], cx");
    asm("mov al, dh");
    asm("mov ah, 0");
    asm("mov [g_month], ax");
    asm("mov al, dl");
    asm("mov ah, 0");
    asm("mov [g_day], ax");
    asm("push cs");
    asm("pop ds");
}

int dos_set_date(int year, int month, int day)
{
    asm("mov cx, [bp+8]");
    asm("mov dh, [bp+6]");
    asm("mov dl, [bp+4]");
    asm("mov ah, 0x2B");
    asm("int 0x21");
    asm("mov ah, 0");
    asm("push cs");
    asm("pop ds");
}

void dos_get_time(void)
{
    asm("mov ah, 0x2C");
    asm("int 0x21");
    asm("mov al, ch");
    asm("mov ah, 0");
    asm("mov [g_hour], ax");
    asm("mov al, cl");
    asm("mov ah, 0");
    asm("mov [g_min], ax");
    asm("mov al, dh");
    asm("mov ah, 0");
    asm("mov [g_sec], ax");
    asm("push cs");
    asm("pop ds");
}

void dos_set_time(int hour, int min, int sec)
{
    asm("mov ch, [bp+8]");
    asm("mov cl, [bp+6]");
    asm("mov dh, [bp+4]");
    asm("xor dl, dl");
    asm("mov ah, 0x2D");
    asm("int 0x21");
    asm("push cs");
    asm("pop ds");
}

void dos_line_input(char *buf)
{
    asm("mov dx, [bp+4]");
    asm("mov ah, 0x0A");
    asm("int 0x21");
    asm("push cs");
    asm("pop ds");
}

void clear_screen(void)
{
    asm("mov ax, 0x0003");
    asm("int 0x10");
    asm("push cs");
    asm("pop ds");
}

int exec_program(char *path)
{
    asm("push es");
    asm("push ds");
    asm("pop es");
    asm("mov dx, [bp+4]");
    asm("mov bx, offset exec_pb");
    asm("mov ax, 0x4B00");
    asm("int 0x21");
    asm("jnc Lcmd_exec_ok");
    asm("mov ax, 0xFFFF");
    asm("Lcmd_exec_ok:");
    asm("pop es");
    asm("push cs");
    asm("pop ds");
}

void str_clear(char *s)
{
    buf_set(s, 0, 0);
}

void str_copy(char *dst, char *src, int max)
{
    int i;
    int c;
    i = 0;
    while (i < max - 1) {
        c = buf_get(src, i);
        buf_set(dst, i, c);
        if (c == 0) {
            return;
        }
        i = i + 1;
    }
    buf_set(dst, i, 0);
}

int str_len(char *s)
{
    int i;
    i = 0;
    while (buf_get(s, i) != 0) {
        i = i + 1;
    }
    return i;
}

int str_eq(char *a, char *b)
{
    int i;
    int x;
    int y;
    i = 0;
    while (1) {
        x = toupper_ch(buf_get(a, i));
        y = toupper_ch(buf_get(b, i));
        if (x != y) {
            return 0;
        }
        if (x == 0) {
            return 1;
        }
        i = i + 1;
    }
}

int str_has(char *s, int c)
{
    int i;
    i = 0;
    while (buf_get(s, i) != 0) {
        if (buf_get(s, i) == c) {
            return 1;
        }
        i = i + 1;
    }
    return 0;
}

void skip_spaces(void)
{
    while (peek_byte(cursor) == ' ' || peek_byte(cursor) == 9) {
        cursor = cursor + 1;
    }
}

int next_token(char *out, int max)
{
    int i;
    int c;
    skip_spaces();
    i = 0;
    while (1) {
        c = peek_byte(cursor);
        if (c == 0 || c == ' ' || c == 9) {
            break;
        }
        if (i < max - 1) {
            buf_set(out, i, toupper_ch(c));
            i = i + 1;
        }
        cursor = cursor + 1;
    }
    buf_set(out, i, 0);
    if (i == 0) {
        return 0;
    }
    return 1;
}

void print_crlf(void)
{
    print_dollar("\r\n$");
}

void show_prompt(void)
{
    int i;
    int c;
    int drive;
    i = 0;
    while (1) {
        c = buf_get(prompt_fmt, i);
        if (c == 0) break;
        if (c == '$') {
            i = i + 1;
            c = buf_get(prompt_fmt, i);
            if (c == 'p' || c == 'P') {
                drive = dos_current_drive();
                print_char(drive + 'A');
                print_dollar(":$");
                get_cwd(cwd);
                print_string(cwd);
            } else if (c == 'n' || c == 'N') {
                drive = dos_current_drive();
                print_char(drive + 'A');
            } else if (c == 'g' || c == 'G') {
                print_char('>');
            } else if (c == 'l' || c == 'L') {
                print_char('<');
            } else if (c == 'b' || c == 'B') {
                print_char('|');
            } else if (c == 'q' || c == 'Q') {
                print_char('=');
            } else if (c == '$') {
                print_char('$');
            } else if (c == '_') {
                print_crlf();
            } else if (c == 'd' || c == 'D') {
                dos_get_date();
                print_num(g_month);
                print_char('-');
                print_num(g_day);
                print_char('-');
                print_num(g_year);
            } else if (c == 't' || c == 'T') {
                dos_get_time();
                print_num(g_hour);
                print_char(':');
                print_num(g_min);
                print_char(':');
                print_num(g_sec);
            }
            if (c != 0) i = i + 1;
        } else {
            print_char(c);
            i = i + 1;
        }
    }
    print_char(' ');
}

void do_type(void)
{
    int h;
    int n;
    if (!next_token(arg1, PATH_MAX)) {
        print_dollar("TYPE file\r\n$");
        return;
    }
    h = dos_open(arg1, 0);
    if (h == -1) {
        print_dollar("file not found\r\n$");
        return;
    }
    n = dos_read(h, copybuf, 128);
    while (n > 0) {
        dos_write(1, copybuf, n);
        n = dos_read(h, copybuf, 128);
    }
    dos_close(h);
}

void do_copy(void)
{
    int in;
    int out;
    int n;
    if (!next_token(arg1, PATH_MAX) || !next_token(arg2, PATH_MAX)) {
        print_dollar("COPY src dst\r\n$");
        return;
    }
    in = dos_open(arg1, 0);
    if (in == -1) {
        print_dollar("COPY src dst\r\n$");
        return;
    }
    out = dos_create(arg2, 0);
    if (out == -1) {
        dos_close(in);
        print_dollar("COPY src dst\r\n$");
        return;
    }
    n = dos_read(in, copybuf, 128);
    while (n > 0) {
        dos_write(out, copybuf, n);
        n = dos_read(in, copybuf, 128);
    }
    dos_close(out);
    dos_close(in);
    print_dollar("copied\r\n$");
}

void do_dir(void)
{
    int i;
    int attr;
    int size;
    skip_spaces();
    if (peek_byte(cursor) == 0) {
        str_copy(pattern, "*.*", PATH_MAX);
    } else {
        next_token(pattern, PATH_MAX);
        if (!str_has(pattern, '*') && !str_has(pattern, '?') && !str_has(pattern, '.')) {
            i = str_len(pattern);
            buf_set(pattern, i, '\\');
            buf_set(pattern, i + 1, '*');
            buf_set(pattern, i + 2, '.');
            buf_set(pattern, i + 3, '*');
            buf_set(pattern, i + 4, 0);
        }
    }
    dos_set_dta(dta);
    print_dollar(" Directory of A:\\\r\n$");
    dir_count = 0;
    dir_bytes = 0;
    if (dos_find_first(pattern, 0x10) == -1) {
        print_dollar("File not found\r\n$");
        return;
    }
    while (1) {
        print_string(buf_addr(dta, 0x1e));
        i = str_len(buf_addr(dta, 0x1e));
        while (i < 13) {
            print_char(' ');
            i = i + 1;
        }
        attr = buf_get(dta, 0x15);
        if (attr & 0x10) {
            print_dollar("<DIR>$");
        } else {
            size = buf_get(dta, 0x1a) + (buf_get(dta, 0x1b) * 256);
            print_num(size);
            dir_bytes = dir_bytes + size;
        }
        print_crlf();
        if (!(buf_get(dta, 0x1e) == '.' && (buf_get(dta, 0x1f) == 0 || (buf_get(dta, 0x1f) == '.' && buf_get(dta, 0x20) == 0)))) {
            dir_count = dir_count + 1;
        }
        if (dos_find_next() == -1) {
            break;
        }
    }
    print_dollar("        $");
    print_num(dir_count);
    print_dollar(" File(s)     $");
    print_num(dir_bytes);
    print_dollar(" bytes\r\n                    $");
    if (dos_disk_free(0) == 0) {
        /* free_clusters * secs/clust * bytes/sect — low 16 bits is enough for the gate */
        size = dos_df_free_clusters() * dos_df_secs_per_clust() * dos_df_bytes_per_sect();
        if (size < 0) {
            size = 0;
        }
        print_num(size);
    } else {
        print_char('0');
    }
    print_dollar(" bytes free\r\n$");
}

void make_exec_tail(void)
{
    int i;
    int n;
    skip_spaces();
    /* DOS command tail: length, optional leading space, text, CR. */
    i = 0;
    buf_set(exec_tail, 1, ' ');
    i = 1;
    while (peek_byte(cursor) != 0 && i < 126) {
        buf_set(exec_tail, i + 1, peek_byte(cursor));
        cursor = cursor + 1;
        i = i + 1;
    }
    buf_set(exec_tail, 0, i);
    buf_set(exec_tail, i + 1, 13);
    n = 0;
    while (n < 16) {
        buf_set(fcb1, n, 0);
        buf_set(fcb2, n, 0);
        n = n + 1;
    }
    n = 0;
    while (n < 14) {
        buf_set(exec_pb, n, 0);
        n = n + 1;
    }
    /* env seg, cmd tail ptr, FCB1, FCB2 */
    asm("mov ax, [0x2C]");
    asm("mov word ptr exec_pb, ax");
    asm("mov word ptr exec_pb+2, offset exec_tail");
    asm("mov ax, ds");
    asm("mov word ptr exec_pb+4, ax");
    asm("mov word ptr exec_pb+6, offset fcb1");
    asm("mov word ptr exec_pb+8, ax");
    asm("mov word ptr exec_pb+10, offset fcb2");
    asm("mov word ptr exec_pb+12, ax");
}

int has_dot(char *s)
{
    return str_has(s, '.');
}

int try_exec_name(void)
{
    int i;
    if (exec_program(prog) == 0) {
        last_errorlevel = dos_exit_code();
        return 1;
    }
    if (!has_dot(prog)) {
        i = str_len(prog);
        buf_set(prog, i, '.');
        buf_set(prog, i + 1, 'C');
        buf_set(prog, i + 2, 'O');
        buf_set(prog, i + 3, 'M');
        buf_set(prog, i + 4, 0);
        if (exec_program(prog) == 0) {
            last_errorlevel = dos_exit_code();
            return 1;
        }
        buf_set(prog, i + 1, 'E');
        buf_set(prog, i + 2, 'X');
        buf_set(prog, i + 3, 'E');
        if (exec_program(prog) == 0) {
            last_errorlevel = dos_exit_code();
            return 1;
        }
    }
    return 0;
}

void do_set(void)
{
    int i;
    int c;
    skip_spaces();
    if (peek_byte(cursor) == 0) {
        if (buf_get(last_set, 0) != 0) {
            print_string(last_set);
            print_crlf();
        }
        return;
    }
    i = 0;
    while (peek_byte(cursor) != 0 && i < 78) {
        c = peek_byte(cursor);
        buf_set(last_set, i, c);
        i = i + 1;
        cursor = cursor + 1;
    }
    buf_set(last_set, i, 0);
}

void do_prompt(void)
{
    int i;
    int c;
    skip_spaces();
    if (peek_byte(cursor) == 0) {
        print_string(prompt_fmt);
        print_crlf();
        return;
    }
    i = 0;
    while (peek_byte(cursor) != 0 && i < 78) {
        c = peek_byte(cursor);
        buf_set(prompt_fmt, i, c);
        i = i + 1;
        cursor = cursor + 1;
    }
    buf_set(prompt_fmt, i, 0);
}

static int parse_pos;

int parse_uint(char *s)
{
    int n;
    int c;
    n = 0;
    while (1) {
        c = buf_get(s, parse_pos);
        if (c < '0' || c > '9') break;
        n = n * 10 + (c - '0');
        parse_pos = parse_pos + 1;
    }
    return n;
}

void do_date(void)
{
    int month;
    int day;
    int year;
    int rc;
    skip_spaces();
    if (peek_byte(cursor) == 0) {
        dos_get_date();
        print_dollar("Current date is $");
        print_num(g_month);
        print_char('-');
        print_num(g_day);
        print_char('-');
        print_num(g_year);
        print_crlf();
        return;
    }
    if (!next_token(arg1, PATH_MAX)) return;
    parse_pos = 0;
    month = parse_uint(arg1);
    if (buf_get(arg1, parse_pos) == '-' || buf_get(arg1, parse_pos) == '/') parse_pos = parse_pos + 1;
    day = parse_uint(arg1);
    if (buf_get(arg1, parse_pos) == '-' || buf_get(arg1, parse_pos) == '/') parse_pos = parse_pos + 1;
    year = parse_uint(arg1);
    rc = dos_set_date(year, month, day);
    if (rc == 255) print_dollar("Invalid date\r\n$");
}

void do_time(void)
{
    int hour;
    int min;
    int sec;
    skip_spaces();
    if (peek_byte(cursor) == 0) {
        dos_get_time();
        print_dollar("Current time is $");
        print_num(g_hour);
        print_char(':');
        print_num(g_min);
        print_char(':');
        print_num(g_sec);
        print_crlf();
        return;
    }
    if (!next_token(arg1, PATH_MAX)) return;
    parse_pos = 0;
    hour = parse_uint(arg1);
    if (buf_get(arg1, parse_pos) == ':') parse_pos = parse_pos + 1;
    min = parse_uint(arg1);
    if (buf_get(arg1, parse_pos) == ':') parse_pos = parse_pos + 1;
    sec = parse_uint(arg1);
    dos_set_time(hour, min, sec);
}

void do_vol(void)
{
    dos_set_dta(dta);
    if (dos_find_first("*.*", 8) == -1) {
        print_dollar("Volume in drive A has no label\r\n$");
        return;
    }
    print_dollar("Volume in drive A is $");
    print_string(buf_addr(dta, 0x1E));
    print_crlf();
}

void do_verify(void)
{
    skip_spaces();
    if (!next_token(arg1, PATH_MAX)) {
        if (verify_on) print_dollar("VERIFY is ON\r\n$");
        else print_dollar("VERIFY is OFF\r\n$");
        return;
    }
    if (str_eq(arg1, "ON")) {
        verify_on = 1;
        dos_set_verify(1);
    } else if (str_eq(arg1, "OFF")) {
        verify_on = 0;
        dos_set_verify(0);
    } else {
        print_dollar("VERIFY ON|OFF\r\n$");
    }
}

void do_break(void)
{
    skip_spaces();
    if (!next_token(arg1, PATH_MAX)) {
        break_on = dos_get_break();
        if (break_on) print_dollar("BREAK is ON\r\n$");
        else print_dollar("BREAK is OFF\r\n$");
        return;
    }
    if (str_eq(arg1, "ON")) {
        break_on = 1;
        dos_set_break(1);
        print_dollar("BREAK OK\r\n$");
    } else if (str_eq(arg1, "OFF")) {
        break_on = 0;
        dos_set_break(0);
        print_dollar("BREAK OK\r\n$");
    } else {
        print_dollar("BREAK?\r\n$");
    }
}

void clear_batch_args(void)
{
    int i;
    i = 0;
    while (i < 160) {
        buf_set(batch_args, i, 0);
        i = i + 1;
    }
    batch_argc = 0;
}

void collect_batch_args(void)
{
    clear_batch_args();
    while (batch_argc < 9) {
        if (!next_token(arg2, 16)) {
            break;
        }
        str_copy(buf_addr(batch_args, batch_argc * 16), arg2, 16);
        batch_argc = batch_argc + 1;
    }
}

void do_shift(void)
{
    int i;
    int j;
    i = 0;
    while (i < 8) {
        j = 0;
        while (j < 16) {
            buf_set(batch_args, i * 16 + j, buf_get(batch_args, (i + 1) * 16 + j));
            j = j + 1;
        }
        i = i + 1;
    }
    j = 0;
    while (j < 16) {
        buf_set(batch_args, 8 * 16 + j, 0);
        j = j + 1;
    }
    if (batch_argc > 0) {
        batch_argc = batch_argc - 1;
    }
    print_dollar("SHIFT OK\r\n$");
}

void do_exit(void)
{
    if (batch_depth > 0) {
        batch_abort = 1;
        print_dollar("EXIT OK\r\n$");
        return;
    }
    print_dollar("Primary\r\n$");
}

void expand_batch_args(void)
{
    int i;
    int o;
    int c;
    int n;
    char out[82];
    i = 0;
    o = 0;
    while (buf_get(cmd, i) != 0 && o < 80) {
        c = buf_get(cmd, i);
        if (c == '%' && buf_get(cmd, i + 1) >= '1' && buf_get(cmd, i + 1) <= '9') {
            n = buf_get(cmd, i + 1) - '1';
            i = i + 2;
            {
                int j;
                j = 0;
                while (buf_get(batch_args, n * 16 + j) != 0 && o < 80) {
                    buf_set(out, o, buf_get(batch_args, n * 16 + j));
                    o = o + 1;
                    j = j + 1;
                }
            }
        } else {
            buf_set(out, o, c);
            o = o + 1;
            i = i + 1;
        }
    }
    buf_set(out, o, 0);
    str_copy(cmd, out, LINE_MAX);
}

void do_ctty(void)
{
    int h;
    if (!next_token(arg1, PATH_MAX)) {
        print_dollar("CTTY device\r\n$");
        return;
    }
    h = dos_open(arg1, 2);
    if (h == -1) {
        print_dollar("Invalid device\r\n$");
        return;
    }
    dos_force_dup(h, 0);
    dos_force_dup(h, 1);
    dos_force_dup(h, 2);
    if (h > 2) dos_close(h);
}

void dispatch(void);

void for_run_body(char *value)
{
    int i;
    int o;
    int c;
    int vlen;
    int match;
    int j;
    vlen = str_len(for_var);
    i = 0;
    o = 0;
    while (buf_get(for_body, i) != 0 && o < 80) {
        c = buf_get(for_body, i);
        match = 0;
        if (c == '%') {
            j = 0;
            while (j < vlen && buf_get(for_body, i + 1 + j) == buf_get(for_var, j)) {
                j = j + 1;
            }
            if (j == vlen) {
                match = 1;
                i = i + 1 + vlen;
                j = 0;
                while (buf_get(value, j) != 0 && o < 80) {
                    buf_set(cmd, o, buf_get(value, j));
                    o = o + 1;
                    j = j + 1;
                }
            } else if (buf_get(for_body, i + 1) == '%' && vlen > 0) {
                j = 0;
                while (j < vlen && buf_get(for_body, i + 2 + j) == buf_get(for_var, j)) {
                    j = j + 1;
                }
                if (j == vlen) {
                    match = 1;
                    i = i + 2 + vlen;
                    j = 0;
                    while (buf_get(value, j) != 0 && o < 80) {
                        buf_set(cmd, o, buf_get(value, j));
                        o = o + 1;
                        j = j + 1;
                    }
                }
            }
        }
        if (!match) {
            buf_set(cmd, o, c);
            o = o + 1;
            i = i + 1;
        }
    }
    buf_set(cmd, o, 0);
    dispatch();
}

void do_for(void)
{
    int i;
    int c;
    int wild;
    skip_spaces();
    if (!next_token(arg1, PATH_MAX)) {
        print_dollar("FOR %v IN (set) DO cmd\r\n$");
        return;
    }
    i = 0;
    if (buf_get(arg1, 0) == '%') i = 1;
    if (buf_get(arg1, i) == '%') i = i + 1;
    buf_set(for_var, 0, buf_get(arg1, i));
    buf_set(for_var, 1, 0);
    if (!next_token(arg1, PATH_MAX) || !str_eq(arg1, "IN")) {
        print_dollar("FOR %v IN (set) DO cmd\r\n$");
        return;
    }
    skip_spaces();
    if (peek_byte(cursor) != '(') {
        print_dollar("FOR %v IN (set) DO cmd\r\n$");
        return;
    }
    cursor = cursor + 1;
    i = 0;
    while (peek_byte(cursor) != 0 && peek_byte(cursor) != ')' && i < 62) {
        c = peek_byte(cursor);
        buf_set(for_item, i, c);
        i = i + 1;
        cursor = cursor + 1;
    }
    buf_set(for_item, i, 0);
    if (peek_byte(cursor) == ')') cursor = cursor + 1;
    skip_spaces();
    if (!next_token(arg1, PATH_MAX) || !str_eq(arg1, "DO")) {
        print_dollar("FOR %v IN (set) DO cmd\r\n$");
        return;
    }
    skip_spaces();
    i = 0;
    while (peek_byte(cursor) != 0 && i < 80) {
        buf_set(for_body, i, peek_byte(cursor));
        i = i + 1;
        cursor = cursor + 1;
    }
    buf_set(for_body, i, 0);
    wild = 0;
    i = 0;
    while (buf_get(for_item, i) != 0) {
        c = buf_get(for_item, i);
        if (c == '*' || c == '?') wild = 1;
        i = i + 1;
    }
    if (wild) {
        dos_set_dta(dta);
        if (dos_find_first(for_item, 0x10) == -1) return;
        while (1) {
            for_run_body(buf_addr(dta, 0x1E));
            if (dos_find_next() == -1) break;
        }
        return;
    }
    /* Space/comma-separated literal set. */
    i = 0;
    while (buf_get(for_item, i) != 0) {
        while (buf_get(for_item, i) == ' ' || buf_get(for_item, i) == ',') i = i + 1;
        if (buf_get(for_item, i) == 0) break;
        c = 0;
        while (buf_get(for_item, i) != 0 && buf_get(for_item, i) != ' ' && buf_get(for_item, i) != ',') {
            buf_set(arg2, c, buf_get(for_item, i));
            c = c + 1;
            i = i + 1;
        }
        buf_set(arg2, c, 0);
        for_run_body(arg2);
    }
}

/* Print the value from the most recent NAME=VALUE assignment. */
int print_last_set_value(char *name)
{
    int i;
    int c;
    i = 0;
    while (buf_get(name, i) != 0) {
        c = buf_get(last_set, i);
        if (toupper_ch(c) != toupper_ch(buf_get(name, i))) {
            return 0;
        }
        i = i + 1;
    }
    if (buf_get(last_set, i) != '=') {
        return 0;
    }
    print_string(buf_addr(last_set, i + 1));
    return 1;
}

/* ECHO's argument is expanded here so batch SET values affect later lines. */
void echo_tail(void)
{
    int i;
    int c;
    int closed;
    skip_spaces();
    while (peek_byte(cursor) != 0) {
        c = peek_byte(cursor);
        if (c != '%') {
            print_char(c);
            cursor = cursor + 1;
        } else {
            cursor = cursor + 1;
            i = 0;
            closed = 0;
            while (peek_byte(cursor) != 0 && i < PATH_MAX - 1) {
                c = peek_byte(cursor);
                if (c == '%') {
                    closed = 1;
                    cursor = cursor + 1;
                    break;
                }
                buf_set(arg1, i, c);
                i = i + 1;
                cursor = cursor + 1;
            }
            buf_set(arg1, i, 0);
            if (closed && print_last_set_value(arg1)) {
                /* value was emitted */
            } else {
                print_char('%');
                print_string(arg1);
                if (closed) print_char('%');
            }
        }
    }
    print_crlf();
}

void do_if(void);
void dispatch(void);

void do_batch(char *name)
{
    int h;
    int n;
    int c;
    int slot;
    int base;
    int label;
    if (batch_depth >= BATCH_MAX) {
        return;
    }
    h = dos_open(name, 0);
    if (h == -1) {
        return;
    }
    slot = batch_depth;
    batch_handles[slot] = h;
    base = slot * 32;
    str_copy(buf_addr(batch_names, base), name, 32);
    batch_depth = batch_depth + 1;
    batch_abort = 0;
    n = 0;
    while (1) {
        if (batch_abort) {
            break;
        }
        n = 0;
        while (n < LINE_MAX) {
            c = dos_read(h, copybuf, 1);
            if (c <= 0) {
                break;
            }
            c = buf_get(copybuf, 0);
            if (c == 13) {
                break;
            }
            if (c != 10) {
                buf_set(cmd, n, c);
                n = n + 1;
            }
        }
        if (c <= 0 && n == 0) {
            break;
        }
        buf_set(cmd, n, 0);
        if (buf_get(cmd, 0) == '@') {
            str_copy(cmd, buf_addr(cmd, 1), LINE_MAX);
        }
        expand_batch_args();
        label = 0;
        if (buf_get(cmd, 0) == ':') {
            cursor = buf_addr(cmd, 1);
            next_token(arg1, PATH_MAX);
            label = 1;
            if (goto_active && str_eq(arg1, goto_name)) goto_active = 0;
        }
        if (!goto_active && !label && buf_get(cmd, 0) != 0) {
            dispatch();
        }
        if (c <= 0) {
            break;
        }
    }
    batch_depth = batch_depth - 1;
    dos_close(h);
}

void do_if(void)
{
    int target;
    int exists;
    int h;
    int i;
    int j;
    int eq;
    int negate;
    char left[64];
    char right[64];
    next_token(arg1, PATH_MAX);
    if (str_eq(arg1, "ERRORLEVEL")) {
        next_token(arg1, PATH_MAX);
        target = 0;
        i = 0;
        while (buf_get(arg1, i) >= '0' && buf_get(arg1, i) <= '9') {
            target = target * 10 + buf_get(arg1, i) - '0';
            i = i + 1;
        }
        if (last_errorlevel >= target) {
            skip_spaces();
            str_copy(cmd, buf_addr(cmd, cursor - buf_addr(cmd, 0)), LINE_MAX);
            dispatch();
        }
        return;
    }
    negate = 0;
    exists = 0;
    if (str_eq(arg1, "NOT")) {
        negate = 1;
        exists = 1;
        next_token(arg1, PATH_MAX);
    }
    if (str_eq(arg1, "EXIST")) {
        next_token(arg1, PATH_MAX);
        h = dos_open(arg1, 0);
        if (h != -1) {
            dos_close(h);
            if (!exists) {
                skip_spaces();
                str_copy(cmd, buf_addr(cmd, cursor - buf_addr(cmd, 0)), LINE_MAX);
                dispatch();
            }
        } else {
            if (exists) {
                skip_spaces();
                str_copy(cmd, buf_addr(cmd, cursor - buf_addr(cmd, 0)), LINE_MAX);
                dispatch();
            }
        }
        return;
    }
    /* IF [NOT] string1==string2 command */
    i = 0;
    eq = -1;
    while (buf_get(arg1, i) != 0) {
        if (buf_get(arg1, i) == '=' && buf_get(arg1, i + 1) == '=') {
            eq = i;
            break;
        }
        i = i + 1;
    }
    if (eq < 0) {
        str_copy(left, arg1, 64);
        if (!next_token(arg1, PATH_MAX)) {
            return;
        }
        if (!str_eq(arg1, "==")) {
            /* token may be ==right or == alone */
            if (buf_get(arg1, 0) == '=' && buf_get(arg1, 1) == '=') {
                j = 0;
                i = 2;
                while (buf_get(arg1, i) != 0) {
                    buf_set(right, j, buf_get(arg1, i));
                    j = j + 1;
                    i = i + 1;
                }
                buf_set(right, j, 0);
            } else {
                return;
            }
        } else {
            if (!next_token(right, 64)) {
                return;
            }
        }
    } else {
        j = 0;
        i = 0;
        while (i < eq) {
            buf_set(left, j, buf_get(arg1, i));
            j = j + 1;
            i = i + 1;
        }
        buf_set(left, j, 0);
        j = 0;
        i = eq + 2;
        while (buf_get(arg1, i) != 0) {
            buf_set(right, j, buf_get(arg1, i));
            j = j + 1;
            i = i + 1;
        }
        buf_set(right, j, 0);
    }
    eq = str_eq(left, right);
    if (negate) {
        if (!eq) {
            skip_spaces();
            str_copy(cmd, buf_addr(cmd, cursor - buf_addr(cmd, 0)), LINE_MAX);
            dispatch();
        }
    } else {
        if (eq) {
            skip_spaces();
            str_copy(cmd, buf_addr(cmd, cursor - buf_addr(cmd, 0)), LINE_MAX);
            dispatch();
        }
    }
}

int setup_redirection(void)
{
    int h;
    if (redir_kind == 0) {
        return 1;
    }
    if (redir_kind == 3) {
        h = dos_open(redir_name, 0);
        if (h == -1) return 0;
        saved_stdin = dos_dup(0);
        if (saved_stdin == -1) return 0;
        dos_force_dup(h, 0);
    } else {
        if (redir_kind == 2) {
            h = dos_open(redir_name, 1);
            if (h != -1) dos_seek_end(h);
        } else {
            h = -1;
        }
        if (h == -1) h = dos_create(redir_name, 0);
        if (h == -1) return 0;
        saved_stdout = dos_dup(1);
        if (saved_stdout == -1) return 0;
        dos_force_dup(h, 1);
    }
    redir_handle = h;
    return 1;
}

void restore_redirection(void)
{
    if (redir_kind == 3) {
        dos_force_dup(saved_stdin, 0);
        dos_close(saved_stdin);
    }
    if (redir_kind == 1 || redir_kind == 2) {
        dos_force_dup(saved_stdout, 1);
        dos_close(saved_stdout);
    }
    if (redir_kind != 0) dos_close(redir_handle);
}

void parse_redirection(void)
{
    int i;
    int c;
    int n;
    redir_kind = 0;
    i = 0;
    while (buf_get(cmd, i) != 0) {
        c = buf_get(cmd, i);
        if (c == '>' || c == '<') {
            if (c == '>') redir_kind = 1;
            else redir_kind = 3;
            buf_set(cmd, i, 0);
            i = i + 1;
            if (c == '>' && buf_get(cmd, i) == '>') {
                redir_kind = 2;
                i = i + 1;
            }
            while (buf_get(cmd, i) == ' ') i = i + 1;
            n = 0;
            while (buf_get(cmd, i) != 0 && buf_get(cmd, i) != ' ' && n < PATH_MAX - 1) {
                buf_set(redir_name, n, buf_get(cmd, i));
                n = n + 1;
                i = i + 1;
            }
            buf_set(redir_name, n, 0);
            return;
        }
        i = i + 1;
    }
}

void dispatch_plain(void)
{
    int h;
    int i;
    cursor = buf_addr(cmd, 0);
    if (!next_token(prog, PATH_MAX)) return;
    if (str_eq(prog, "DIR")) { do_dir(); return; }
    if (str_eq(prog, "TYPE")) { do_type(); return; }
    if (str_eq(prog, "COPY")) { do_copy(); return; }
    if (str_eq(prog, "DEL")) {
        if (next_token(arg1, PATH_MAX) && dos_delete(arg1) == 0) print_dollar("deleted\r\n$");
        else print_dollar("DEL file\r\n$");
        return;
    }
    if (str_eq(prog, "CLS")) { clear_screen(); return; }
    if (str_eq(prog, "CD") || str_eq(prog, "CHDIR")) {
        if (next_token(arg1, PATH_MAX)) {
            if (dos_chdir(arg1) == -1) print_dollar("Invalid directory\r\n$");
        } else {
            print_dollar("A:\\$"); get_cwd(cwd); print_string(cwd); print_crlf();
        }
        return;
    }
    if (str_eq(prog, "MD") || str_eq(prog, "MKDIR")) {
        if (!next_token(arg1, PATH_MAX) || dos_mkdir(arg1) == -1) print_dollar("Unable to create directory\r\n$");
        return;
    }
    if (str_eq(prog, "RD") || str_eq(prog, "RMDIR")) {
        if (!next_token(arg1, PATH_MAX) || dos_rmdir(arg1) == -1) print_dollar("Invalid path, not directory,\r\nor directory not empty\r\n$");
        return;
    }
    if (str_eq(prog, "REN") || str_eq(prog, "RENAME")) {
        if (!next_token(arg1, PATH_MAX) || !next_token(arg2, PATH_MAX) || dos_rename(arg1, arg2) == -1) print_dollar("RENAME old new\r\n$");
        return;
    }
    if (str_eq(prog, "ECHO")) {
        echo_tail(); return;
    }
    if (str_eq(prog, "PAUSE")) { print_dollar("Press any key to continue . . .$"); read_key(); print_crlf(); return; }
    if (str_eq(prog, "VER")) { h = dos_version(); print_dollar("rmDOS DOS $"); print_num(h & 255); print_char('.'); print_num(h >> 8); print_crlf(); return; }
    if (str_eq(prog, "SET")) { do_set(); return; }
    if (str_eq(prog, "PROMPT")) { do_prompt(); return; }
    if (str_eq(prog, "DATE")) { do_date(); return; }
    if (str_eq(prog, "TIME")) { do_time(); return; }
    if (str_eq(prog, "VOL")) { do_vol(); return; }
    if (str_eq(prog, "VERIFY")) { do_verify(); return; }
    if (str_eq(prog, "BREAK")) { do_break(); return; }
    if (str_eq(prog, "SHIFT")) { do_shift(); return; }
    if (str_eq(prog, "EXIT")) { do_exit(); return; }
    if (str_eq(prog, "CTTY")) { do_ctty(); return; }
    if (str_eq(prog, "FOR")) { do_for(); return; }
    if (str_eq(prog, "IF")) { do_if(); return; }
    if (str_eq(prog, "CALL")) {
        if (next_token(arg1, PATH_MAX)) {
            collect_batch_args();
            do_batch(arg1);
        }
        return;
    }
    if (str_eq(prog, "GOTO")) {
        if (next_token(goto_name, PATH_MAX)) goto_active = 1;
        return;
    }
    if (str_has(prog, '.') && str_eq(buf_addr(prog, str_len(prog) - 4), ".BAT")) {
        collect_batch_args();
        do_batch(prog);
        return;
    }
    str_copy(prog_base, prog, PATH_MAX);
    make_exec_tail();
    if (!try_exec_name()) {
        if (!str_has(prog_base, '\\') && !str_has(prog_base, '/')) {
            str_copy(prog, "A:\\BIN\\", PATH_MAX);
            i = str_len(prog);
            str_copy(buf_addr(prog, i), prog_base, PATH_MAX - i);
            if (try_exec_name()) return;
        }
        print_dollar("Bad command\r\n$");
    }
}

int run_pipe(void)
{
    int i;
    int h;
    i = 0;
    while (buf_get(cmd, i) != 0 && buf_get(cmd, i) != '|') i = i + 1;
    if (buf_get(cmd, i) == 0) return 0;
    buf_set(cmd, i, 0);
    str_copy(pipe_rhs, buf_addr(cmd, i + 1), LINE_MAX);
    h = dos_create("A:\\PIPE.$$$", 0);
    if (h == -1) return 1;
    saved_stdout = dos_dup(1);
    if (saved_stdout != -1) {
        dos_force_dup(h, 1);
        dispatch();
        dos_force_dup(saved_stdout, 1);
        dos_close(saved_stdout);
    }
    dos_close(h);
    h = dos_open("A:\\PIPE.$$$", 0);
    if (h != -1) {
        saved_stdin = dos_dup(0);
        if (saved_stdin != -1) {
            dos_force_dup(h, 0);
            str_copy(cmd, pipe_rhs, LINE_MAX);
            dispatch();
            dos_force_dup(saved_stdin, 0);
            dos_close(saved_stdin);
        }
        dos_close(h);
    }
    dos_delete("A:\\PIPE.$$$");
    return 1;
}

void dispatch(void)
{
    if (!run_pipe()) {
        parse_redirection();
        if (setup_redirection()) {
            dispatch_plain();
            restore_redirection();
        }
    }
}

void main_loop(void)
{
    int n;
    while (1) {
        reload_ds();
        show_prompt();
        buf_set(line, 0, LINE_MAX);
        buf_set(line, 1, 0);
        dos_line_input(line);
        n = buf_get(line, 1);
        if (n > 0) {
            buf_set(line, n + 2, 0);
            str_copy(cmd, buf_addr(line, 2), LINE_MAX);
            dispatch();
        }
    }
}

int main(void)
{
    reload_ds();
    do_batch("AUTOEXEC.BAT");
    main_loop();
    return 0;
}
