/*
 * COMMAND.COM for rmDOS, built with the in-tree Small-C compiler.
 * Keep this file within the intentionally small wcc language subset.
 */
#include "dos.h"

#define LINE_MAX 80
#define PATH_MAX 64
#define DTA_SIZE 128
#define BATCH_MAX 8
#define DIR_ENT_SIZE 24
#define DIR_MAX_ENTS 80

static char line[82] = { 0 };
static char cmd[82] = { 0 };
static char prog[80] = { 0 };
static char prog_base[64] = { 0 };
static char arg1[64] = { 0 };
static char arg2[64] = { 0 };
static char cwd[64] = { 0 };
static char pattern[64] = { 0 };
static char dta[DTA_SIZE] = { 0 };
static char delete_path[64] = { 0 };
static char copybuf[64] = { 0 };
static char pipe_rhs[82] = { 0 };
static char redir_in_name[64] = { 0 };
static char redir_out_name[64] = { 0 };
static char exec_tail[96] = { 0 };
static char exec_pb[14] = { 0 };
static char fcb1[16] = { 0 };
static char fcb2[16] = { 0 };
static char batch_names[256] = { 0 };
static char batch_args[160] = { 0 };
static char batch_arg0[32] = { 0 };
static char goto_name[64] = { 0 };
static char last_set[80] = { 0 };
static char env_name[32];
static char env_pathbuf[128];
static char env_join[PATH_MAX];
static char prompt_fmt[80] = { '$', 'p', '$', 'g', 0 };
static char for_var[4] = { 0 };
static char for_body[82] = { 0 };
static char for_item[64] = { 0 };
static char for_var_frames[32] = { 0 };
static char for_body_frames[656] = { 0 };
static char for_item_frames[512] = { 0 };
static char if_then[82] = { 0 };
static char if_else[82] = { 0 };
static int cursor;
static int last_errorlevel;
static int batch_depth;
static int batch_handles[8] = { 0 };
static int goto_active;
static int redir_in_kind;
static int redir_out_kind;
static int redir_in_handle;
static int redir_out_handle;
static int saved_stdin;
static int saved_stdout;
static int dir_count;
static int dir_bytes;
static int dir_bytes_hi;
static char dir_pool[1920];
static int dir_nent;
static char dir_keys[4];
static char dir_revs[4];
static int dir_nkeys;
static int verify_on;
static int break_on;
static int batch_argc;
static int batch_abort;
static int echo_on;
static int ctty_saved0;
static int ctty_saved1;
static int ctty_saved2;
static int ctty_active;
static char pipe_tmp[16];
static char pipe_input[16];
static int pipe_seq;
static char batch_arg_frames[1280] = { 0 };
static int batch_argc_frames[8] = { 0 };
static int batch_arg_depth;
static int for_depth;
static int permanent_shell;
static int env_paras;


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

int dos_create_new(char *path, int attr)
{
    asm("mov dx, [bp+6]");
    asm("mov cx, [bp+4]");
    asm("mov ah, 0x5B");
    asm("int 0x21");
    asm("jnc Lcmd_cnew_ok");
    asm("mov ax, 0xFFFF");
    asm("Lcmd_cnew_ok:");
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

int dos_seek_start(int handle)
{
    asm("mov bx, [bp+4]");
    asm("mov ax, 0x4200");
    asm("xor cx, cx");
    asm("xor dx, dx");
    asm("int 0x21");
    asm("jnc Lcmd_seek0_ok");
    asm("mov ax, 0xFFFF");
    asm("Lcmd_seek0_ok:");
}

int dos_current_drive(void)
{
    asm("mov ah, 0x19");
    asm("int 0x21");
    asm("mov ah, 0");
}

/* AH=0Eh select drive DL (0=A). Always returns; unavailable letters stay put. */
void dos_set_drive(int drive)
{
    asm("mov dl, byte ptr [bp+4]");
    asm("mov ah, 0x0E");
    asm("int 0x21");
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
    int ver;
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
            } else if (c == 'e' || c == 'E') {
                print_char(27);
            } else if (c == 'h' || c == 'H') {
                print_char(8);
                print_char(' ');
                print_char(8);
            } else if (c == 'v' || c == 'V') {
                ver = dos_version();
                print_dollar("rmDOS $");
                print_num(ver & 255);
                print_char('.');
                print_num(ver >> 8);
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
        h = 0;
    } else {
        h = dos_open(arg1, 0);
        if (h == -1) {
            print_dollar("file not found\r\n$");
            last_errorlevel = 1;
            return;
        }
    }
    n = dos_read(h, copybuf, 128);
    while (n > 0) {
        dos_write(1, copybuf, n);
        n = dos_read(h, copybuf, 128);
    }
    if (h != 0) dos_close(h);
    last_errorlevel = 0;
}

/* Internal COPY removed — bare COPY PATH-execs BIN\COPY.COM (/V /A /B). */

void print_two_digits(int n)
{
    if (n < 10) {
        print_char('0');
    }
    print_num(n);
}

void print_dta_datetime(void)
{
    int date;
    int time;
    int month;
    int day;
    int year;
    int hour;
    int minute;

    time = peek_word(buf_addr(dta, 0x16));
    date = peek_word(buf_addr(dta, 0x18));
    day = date & 31;
    month = (date >> 5) & 15;
    year = ((date >> 9) & 127) + 1980;
    hour = (time >> 11) & 31;
    minute = (time >> 5) & 63;

    print_dollar("  $");
    print_two_digits(month);
    print_char('-');
    print_two_digits(day);
    print_char('-');
    print_num(year);
    print_char(' ');
    print_two_digits(hour);
    print_char(':');
    print_two_digits(minute);
}

int is_all_files_pattern(char *spec)
{
    int i;
    int base;
    int c;
    i = 0;
    base = 0;
    while (1) {
        c = buf_get(spec, i);
        if (c == 0) {
            break;
        }
        if (c == '\\' || c == '/' || c == ':') {
            base = i + 1;
        }
        i = i + 1;
    }
    return buf_get(spec, base) == '*' &&
           buf_get(spec, base + 1) == '.' &&
           buf_get(spec, base + 2) == '*' &&
           buf_get(spec, base + 3) == 0;
}

void build_delete_path(char *spec, char *found)
{
    int i;
    int base;
    int out;
    int c;
    i = 0;
    base = 0;
    while (1) {
        c = buf_get(spec, i);
        if (c == 0) {
            break;
        }
        if (c == '\\' || c == '/' || c == ':') {
            base = i + 1;
        }
        i = i + 1;
    }
    out = 0;
    while (out < base && out < PATH_MAX - 1) {
        buf_set(delete_path, out, buf_get(spec, out));
        out = out + 1;
    }
    i = 0;
    while (buf_get(found, i) != 0 && out < PATH_MAX - 1) {
        buf_set(delete_path, out, buf_get(found, i));
        out = out + 1;
        i = i + 1;
    }
    buf_set(delete_path, out, 0);
}

void do_del(void)
{
    int c;
    int deleted;
    int failed;

    if (!next_token(arg1, PATH_MAX)) {
        print_dollar("DEL file\r\n$");
        last_errorlevel = 1;
        return;
    }
    if (is_all_files_pattern(arg1) && batch_depth == 0) {
        print_dollar("Are you sure (Y/N)? $");
        c = toupper_ch(read_key());
        print_char(c);
        print_crlf();
        if (c != 'Y') {
            last_errorlevel = 0;
            return;
        }
    }

    dos_set_dta(dta);
    if (dos_find_first(arg1, 0) == -1) {
        print_dollar("DEL file\r\n$");
        last_errorlevel = 1;
        return;
    }
    deleted = 0;
    failed = 0;
    while (1) {
        build_delete_path(arg1, buf_addr(dta, 0x1E));
        if (dos_delete(delete_path) == 0) {
            deleted = deleted + 1;
        } else {
            failed = 1;
        }
        if (dos_find_next() == -1) {
            break;
        }
    }
    if (deleted > 0) {
        print_dollar("deleted\r\n$");
    }
    if (deleted == 0 || failed) {
        last_errorlevel = 1;
    } else {
        last_errorlevel = 0;
    }
}

int dir_ent_off(int idx)
{
    return idx * DIR_ENT_SIZE;
}

int dir_u16_cmp(int a, int b)
{
    if (a == b) {
        return 0;
    }
    if ((a & 0x8000) == (b & 0x8000)) {
        if (a < b) {
            return -1;
        }
        return 1;
    }
    if (a & 0x8000) {
        return 1;
    }
    return -1;
}

int dir_name_cmp(int a, int b)
{
    int i;
    int ca;
    int cb;
    int oa;
    int ob;

    oa = dir_ent_off(a);
    ob = dir_ent_off(b);
    i = 0;
    while (i < 13) {
        ca = buf_get(dir_pool, oa + i);
        cb = buf_get(dir_pool, ob + i);
        if (ca != cb) {
            if (ca < cb) {
                return -1;
            }
            return 1;
        }
        if (ca == 0) {
            return 0;
        }
        i = i + 1;
    }
    return 0;
}

int dir_ext_at(int off)
{
    int i;
    int c;

    i = 0;
    while (i < 13) {
        c = buf_get(dir_pool, off + i);
        if (c == 0) {
            return off + i;
        }
        if (c == '.') {
            return off + i + 1;
        }
        i = i + 1;
    }
    return off + i;
}

int dir_ext_cmp(int a, int b)
{
    int i;
    int ca;
    int cb;
    int oa;
    int ob;

    oa = dir_ext_at(dir_ent_off(a));
    ob = dir_ext_at(dir_ent_off(b));
    i = 0;
    while (i < 13) {
        ca = buf_get(dir_pool, oa + i);
        cb = buf_get(dir_pool, ob + i);
        if (ca != cb) {
            if (ca < cb) {
                return -1;
            }
            return 1;
        }
        if (ca == 0) {
            return 0;
        }
        i = i + 1;
    }
    return 0;
}

int dir_key_cmp(int a, int b, int key)
{
    int oa;
    int ob;
    int da;
    int db;
    int r;

    oa = dir_ent_off(a);
    ob = dir_ent_off(b);
    if (key == 'N') {
        return dir_name_cmp(a, b);
    }
    if (key == 'E') {
        r = dir_ext_cmp(a, b);
        if (r != 0) {
            return r;
        }
        return dir_name_cmp(a, b);
    }
    if (key == 'D') {
        da = peek_word(buf_addr(dir_pool, oa + 16));
        db = peek_word(buf_addr(dir_pool, ob + 16));
        r = dir_u16_cmp(da, db);
        if (r != 0) {
            return r;
        }
        da = peek_word(buf_addr(dir_pool, oa + 14));
        db = peek_word(buf_addr(dir_pool, ob + 14));
        return dir_u16_cmp(da, db);
    }
    if (key == 'S') {
        da = peek_word(buf_addr(dir_pool, oa + 20));
        db = peek_word(buf_addr(dir_pool, ob + 20));
        r = dir_u16_cmp(da, db);
        if (r != 0) {
            return r;
        }
        da = peek_word(buf_addr(dir_pool, oa + 18));
        db = peek_word(buf_addr(dir_pool, ob + 18));
        return dir_u16_cmp(da, db);
    }
    if (key == 'G') {
        da = buf_get(dir_pool, oa + 13) & 0x10;
        db = buf_get(dir_pool, ob + 13) & 0x10;
        if (da != 0 && db == 0) {
            return -1;
        }
        if (da == 0 && db != 0) {
            return 1;
        }
        return 0;
    }
    return 0;
}

int dir_ent_cmp(int a, int b)
{
    int i;
    int r;
    int key;

    i = 0;
    while (i < dir_nkeys) {
        key = buf_get(dir_keys, i);
        r = dir_key_cmp(a, b, key);
        if (r != 0) {
            if (buf_get(dir_revs, i)) {
                return 0 - r;
            }
            return r;
        }
        i = i + 1;
    }
    return dir_name_cmp(a, b);
}

void dir_ent_swap(int a, int b)
{
    int i;
    int t;
    int oa;
    int ob;

    oa = dir_ent_off(a);
    ob = dir_ent_off(b);
    i = 0;
    while (i < DIR_ENT_SIZE) {
        t = buf_get(dir_pool, oa + i);
        buf_set(dir_pool, oa + i, buf_get(dir_pool, ob + i));
        buf_set(dir_pool, ob + i, t);
        i = i + 1;
    }
}

void dir_sort_pool(void)
{
    int i;
    int j;

    i = 0;
    while (i < dir_nent) {
        j = i + 1;
        while (j < dir_nent) {
            if (dir_ent_cmp(i, j) > 0) {
                dir_ent_swap(i, j);
            }
            j = j + 1;
        }
        i = i + 1;
    }
}

void dir_store_dta(void)
{
    int off;
    int i;
    int c;

    if (dir_nent >= DIR_MAX_ENTS) {
        return;
    }
    off = dir_ent_off(dir_nent);
    i = 0;
    while (i < 13) {
        c = buf_get(dta, 0x1e + i);
        buf_set(dir_pool, off + i, c);
        if (c == 0) {
            break;
        }
        i = i + 1;
    }
    while (i < 13) {
        buf_set(dir_pool, off + i, 0);
        i = i + 1;
    }
    buf_set(dir_pool, off + 13, buf_get(dta, 0x15));
    poke_word(buf_addr(dir_pool, off + 14), peek_word(buf_addr(dta, 0x16)));
    poke_word(buf_addr(dir_pool, off + 16), peek_word(buf_addr(dta, 0x18)));
    poke_word(buf_addr(dir_pool, off + 18), peek_word(buf_addr(dta, 0x1A)));
    poke_word(buf_addr(dir_pool, off + 20), peek_word(buf_addr(dta, 0x1C)));
    dir_nent = dir_nent + 1;
}

void dir_load_dta(int idx)
{
    int off;
    int i;
    int c;

    off = dir_ent_off(idx);
    i = 0;
    while (i < 13) {
        c = buf_get(dir_pool, off + i);
        buf_set(dta, 0x1e + i, c);
        i = i + 1;
    }
    buf_set(dta, 0x15, buf_get(dir_pool, off + 13));
    poke_word(buf_addr(dta, 0x16), peek_word(buf_addr(dir_pool, off + 14)));
    poke_word(buf_addr(dta, 0x18), peek_word(buf_addr(dir_pool, off + 16)));
    poke_word(buf_addr(dta, 0x1A), peek_word(buf_addr(dir_pool, off + 18)));
    poke_word(buf_addr(dta, 0x1C), peek_word(buf_addr(dir_pool, off + 20)));
}

void dir_parse_o_keys(void)
{
    int c;
    int rev;

    dir_nkeys = 0;
    c = peek_byte(cursor);
    if (c == ':') {
        cursor = cursor + 1;
        c = peek_byte(cursor);
    }
    while (dir_nkeys < 4) {
        rev = 0;
        c = peek_byte(cursor);
        if (c == '-') {
            rev = 1;
            cursor = cursor + 1;
            c = peek_byte(cursor);
        }
        if (c >= 'a' && c <= 'z') {
            c = c - 32;
        }
        if (c == 'N' || c == 'E' || c == 'D' || c == 'S' || c == 'G') {
            buf_set(dir_keys, dir_nkeys, c);
            buf_set(dir_revs, dir_nkeys, rev);
            dir_nkeys = dir_nkeys + 1;
            cursor = cursor + 1;
        } else {
            break;
        }
    }
    if (dir_nkeys == 0) {
        buf_set(dir_keys, 0, 'N');
        buf_set(dir_revs, 0, 0);
        dir_nkeys = 1;
    }
}

void do_dir(void)
{
    int i;
    int attr;
    int size;
    int size_hi;
    int old_size;
    int opt_w;
    int opt_p;
    int opt_o;
    int wide_col;
    int page_lines;
    int namelen;
    int have_pat;

    opt_w = 0;
    opt_p = 0;
    opt_o = 0;
    have_pat = 0;
    dir_nent = 0;
    dir_nkeys = 0;
    buf_set(pattern, 0, 0);
    skip_spaces();
    while (peek_byte(cursor) != 0) {
        if (peek_byte(cursor) == '/') {
            cursor = cursor + 1;
            attr = peek_byte(cursor);
            if (attr >= 'a' && attr <= 'z') {
                attr = attr - 32;
            }
            if (attr == 0) {
                last_errorlevel = 1;
                print_dollar("Invalid switch\r\n$");
                return;
            }
            cursor = cursor + 1;
            if (attr == 'W') {
                opt_w = 1;
            } else if (attr == 'P') {
                opt_p = 1;
            } else if (attr == 'O') {
                opt_o = 1;
                dir_parse_o_keys();
            } else {
                last_errorlevel = 1;
                print_dollar("Invalid switch\r\n$");
                return;
            }
            skip_spaces();
        } else {
            if (!next_token(pattern, PATH_MAX)) {
                break;
            }
            have_pat = 1;
            skip_spaces();
        }
    }
    if (!have_pat) {
        str_copy(pattern, "*.*", PATH_MAX);
    } else if (!str_has(pattern, '*') && !str_has(pattern, '?') && !str_has(pattern, '.')) {
        i = str_len(pattern);
        buf_set(pattern, i, '\\');
        buf_set(pattern, i + 1, '*');
        buf_set(pattern, i + 2, '.');
        buf_set(pattern, i + 3, '*');
        buf_set(pattern, i + 4, 0);
    }
    dos_set_dta(dta);
    print_dollar(" Directory of $");
    {
        int drive;
        drive = dos_current_drive();
        print_char(drive + 'A');
        print_char(':');
        get_cwd(cwd);
        print_string(cwd);
    }
    print_crlf();
    dir_count = 0;
    dir_bytes = 0;
    dir_bytes_hi = 0;
    wide_col = 0;
    page_lines = 1;
    if (dos_find_first(pattern, 0x10) == -1) {
        print_dollar("File not found\r\n$");
        last_errorlevel = 1;
        return;
    }
    while (1) {
        if (opt_o) {
            dir_store_dta();
        } else {
            attr = buf_get(dta, 0x15);
            if (opt_w) {
                print_string(buf_addr(dta, 0x1e));
                namelen = str_len(buf_addr(dta, 0x1e));
                while (namelen < 13) {
                    print_char(' ');
                    namelen = namelen + 1;
                }
                wide_col = wide_col + 1;
                if (wide_col >= 5) {
                    print_crlf();
                    wide_col = 0;
                    page_lines = page_lines + 1;
                }
            } else {
                print_string(buf_addr(dta, 0x1e));
                i = str_len(buf_addr(dta, 0x1e));
                while (i < 13) {
                    print_char(' ');
                    i = i + 1;
                }
                if (attr & 0x10) {
                    print_dollar("<DIR>$");
                } else {
                    size = peek_word(buf_addr(dta, 0x1A));
                    size_hi = peek_word(buf_addr(dta, 0x1C));
                    print_u32(size, size_hi);
                }
                print_dta_datetime();
                print_crlf();
                page_lines = page_lines + 1;
            }
            if (!(buf_get(dta, 0x1e) == '.' && (buf_get(dta, 0x1f) == 0 || (buf_get(dta, 0x1f) == '.' && buf_get(dta, 0x20) == 0)))) {
                dir_count = dir_count + 1;
                if (!(attr & 0x10)) {
                    size = peek_word(buf_addr(dta, 0x1A));
                    size_hi = peek_word(buf_addr(dta, 0x1C));
                    old_size = dir_bytes;
                    dir_bytes = dir_bytes + size;
                    dir_bytes_hi = dir_bytes_hi + size_hi;
                    if (dir_bytes < old_size) {
                        dir_bytes_hi = dir_bytes_hi + 1;
                    }
                }
            }
            if (opt_p && page_lines >= 23) {
                print_dollar("Press any key to continue . . .$");
                read_key();
                print_crlf();
                page_lines = 0;
            }
        }
        if (dos_find_next() == -1) {
            break;
        }
    }
    if (opt_o) {
        dir_sort_pool();
        i = 0;
        while (i < dir_nent) {
            dir_load_dta(i);
            attr = buf_get(dta, 0x15);
            if (opt_w) {
                print_string(buf_addr(dta, 0x1e));
                namelen = str_len(buf_addr(dta, 0x1e));
                while (namelen < 13) {
                    print_char(' ');
                    namelen = namelen + 1;
                }
                wide_col = wide_col + 1;
                if (wide_col >= 5) {
                    print_crlf();
                    wide_col = 0;
                    page_lines = page_lines + 1;
                }
            } else {
                print_string(buf_addr(dta, 0x1e));
                namelen = str_len(buf_addr(dta, 0x1e));
                while (namelen < 13) {
                    print_char(' ');
                    namelen = namelen + 1;
                }
                if (attr & 0x10) {
                    print_dollar("<DIR>$");
                } else {
                    size = peek_word(buf_addr(dta, 0x1A));
                    size_hi = peek_word(buf_addr(dta, 0x1C));
                    print_u32(size, size_hi);
                }
                print_dta_datetime();
                print_crlf();
                page_lines = page_lines + 1;
            }
            if (!(buf_get(dta, 0x1e) == '.' && (buf_get(dta, 0x1f) == 0 || (buf_get(dta, 0x1f) == '.' && buf_get(dta, 0x20) == 0)))) {
                dir_count = dir_count + 1;
                if (!(attr & 0x10)) {
                    size = peek_word(buf_addr(dta, 0x1A));
                    size_hi = peek_word(buf_addr(dta, 0x1C));
                    old_size = dir_bytes;
                    dir_bytes = dir_bytes + size;
                    dir_bytes_hi = dir_bytes_hi + size_hi;
                    if (dir_bytes < old_size) {
                        dir_bytes_hi = dir_bytes_hi + 1;
                    }
                }
            }
            if (opt_p && page_lines >= 23) {
                print_dollar("Press any key to continue . . .$");
                read_key();
                print_crlf();
                page_lines = 0;
            }
            i = i + 1;
        }
    }
    if (opt_w && wide_col != 0) {
        print_crlf();
    }
    print_dollar("        $");
    print_u32(dir_count, 0);
    print_dollar(" File(s)     $");
    print_u32(dir_bytes, dir_bytes_hi);
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
    last_errorlevel = 0;
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
        /* Restore basename for .BAT try by caller */
        buf_set(prog, i, 0);
    }
    return 0;
}

void do_if(void);
void dispatch(void);
void do_batch(char *name);
void collect_batch_args(void);
int push_batch_args(void);
void pop_batch_args(void);
void expand_env_percent(void);

/* If prog has no extension, try prog.BAT via the batch interpreter. */
int try_batch_name(void)
{
    int i;
    int h;
    if (has_dot(prog)) {
        return 0;
    }
    i = str_len(prog);
    buf_set(prog, i, '.');
    buf_set(prog, i + 1, 'B');
    buf_set(prog, i + 2, 'A');
    buf_set(prog, i + 3, 'T');
    buf_set(prog, i + 4, 0);
    h = dos_open(prog, 0);
    if (h == -1) {
        buf_set(prog, i, 0);
        return 0;
    }
    dos_close(h);
    if (!push_batch_args()) return 1;
    collect_batch_args();
    do_batch(prog);
    pop_batch_args();
    return 1;
}

/* ECHO. / ECHO: / ECHO/ — classic blank-line forms (no space after ECHO). */
int is_echo_punct(char *p)
{
    int c;
    if (buf_get(p, 0) != 'E') return 0;
    if (buf_get(p, 1) != 'C') return 0;
    if (buf_get(p, 2) != 'H') return 0;
    if (buf_get(p, 3) != 'O') return 0;
    c = buf_get(p, 4);
    if (c == 0) return 0;
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return 0;
    return 1;
}

/* Drive change: "C:" or "c:" */
int is_drive_spec(char *p)
{
    int a;
    int b;
    a = buf_get(p, 0);
    b = buf_get(p, 1);
    if (b != ':') return 0;
    if (buf_get(p, 2) != 0) return 0;
    if (a < 'A' || a > 'Z') return 0;
    return 1;
}

/* Skip one ASCIZ string at seg:off; return offset after NUL. */
int env_skip(int seg, int off)
{
    while (far_peek(seg, off) != 0) off = off + 1;
    return off + 1;
}

int env_name_eq(int seg, int off, char *name)
{
    int i;
    int c;
    int d;
    i = 0;
    while (buf_get(name, i) != 0) {
        c = far_peek(seg, off + i);
        d = buf_get(name, i);
        if (toupper_ch(c) != toupper_ch(d)) return 0;
        i = i + 1;
    }
    if (far_peek(seg, off + i) != '=') return 0;
    return 1;
}

int print_env_value(char *name)
{
    int seg;
    int off;
    int c;
    seg = env_seg();
    if (seg == 0) return 0;
    off = 0;
    while (1) {
        c = far_peek(seg, off);
        if (c == 0) return 0;
        if (env_name_eq(seg, off, name)) {
            off = off + str_len(name) + 1;
            while (1) {
                c = far_peek(seg, off);
                if (c == 0) break;
                print_char(c);
                off = off + 1;
            }
            return 1;
        }
        off = env_skip(seg, off);
    }
}

/* Copy env value for name into buf; 1 if found. */
int env_copy_var(char *name, char *buf, int maxlen)
{
    int seg;
    int off;
    int c;
    int n;
    seg = env_seg();
    if (seg == 0) return 0;
    off = 0;
    while (1) {
        c = far_peek(seg, off);
        if (c == 0) return 0;
        if (env_name_eq(seg, off, name)) {
            off = off + str_len(name) + 1;
            n = 0;
            while (n < maxlen - 1) {
                c = far_peek(seg, off);
                if (c == 0) break;
                buf_set(buf, n, c);
                n = n + 1;
                off = off + 1;
            }
            buf_set(buf, n, 0);
            return 1;
        }
        off = env_skip(seg, off);
    }
}

void env_dump(void)
{
    int seg;
    int off;
    int c;
    seg = env_seg();
    if (seg == 0) return;
    off = 0;
    while (1) {
        c = far_peek(seg, off);
        if (c == 0) break;
        while (1) {
            c = far_peek(seg, off);
            if (c == 0) break;
            print_char(c);
            off = off + 1;
        }
        print_crlf();
        off = off + 1;
    }
}

void env_put(char *name, char *value)
{
    int old;
    int neu;
    int oi;
    int ni;
    int c;
    int skip;
    old = env_seg();
    if (old == 0) return;
    if (env_paras < 16) env_paras = 16;
    if (env_paras > 2048) env_paras = 2048;
    neu = dos_alloc(env_paras);
    if (neu == 0) return;
    oi = 0;
    ni = 0;
    while (1) {
        c = far_peek(old, oi);
        if (c == 0) break;
        skip = env_name_eq(old, oi, name);
        if (!skip) {
            while (1) {
                c = far_peek(old, oi);
                far_poke(neu, ni, c);
                oi = oi + 1;
                ni = ni + 1;
                if (c == 0) break;
            }
        } else {
            oi = env_skip(old, oi);
        }
    }
    if (value != 0 && buf_get(value, 0) != 0) {
        c = 0;
        while (buf_get(name, c) != 0) {
            far_poke(neu, ni, buf_get(name, c));
            ni = ni + 1;
            c = c + 1;
        }
        far_poke(neu, ni, '=');
        ni = ni + 1;
        c = 0;
        while (buf_get(value, c) != 0) {
            far_poke(neu, ni, buf_get(value, c));
            ni = ni + 1;
            c = c + 1;
        }
        far_poke(neu, ni, 0);
        ni = ni + 1;
    }
    far_poke(neu, ni, 0);
    ni = ni + 1;
    oi = oi + 1;
    while (1) {
        c = far_peek(old, oi);
        far_poke(neu, ni, c);
        if (c == 0) break;
        oi = oi + 1;
        ni = ni + 1;
    }
    env_set_seg(neu);
    dos_free(old);
}

void do_set(void)
{
    int i;
    int c;
    int eq;
    skip_spaces();
    if (peek_byte(cursor) == 0) {
        env_dump();
        return;
    }
    i = 0;
    eq = -1;
    while (peek_byte(cursor) != 0 && i < 78) {
        c = peek_byte(cursor);
        buf_set(last_set, i, c);
        if (c == '=' && eq < 0) eq = i;
        i = i + 1;
        cursor = cursor + 1;
    }
    buf_set(last_set, i, 0);
    if (eq < 0) {
        if (print_env_value(last_set)) print_crlf();
        return;
    }
    buf_set(last_set, eq, 0);
    if (eq + 1 >= i) {
        env_put(last_set, 0);
    } else {
        env_put(last_set, buf_addr(last_set, eq + 1));
    }
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
    int n;
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
        if (batch_depth != 0) {
            return;
        }
        print_dollar("Enter new date: $");
        buf_set(line, 0, LINE_MAX);
        buf_set(line, 1, 0);
        dos_line_input(line);
        n = buf_get(line, 1);
        if (n <= 0) {
            return;
        }
        buf_set(line, n + 2, 0);
        str_copy(arg1, buf_addr(line, 2), PATH_MAX);
        parse_pos = 0;
        month = parse_uint(arg1);
        if (buf_get(arg1, parse_pos) == '-' || buf_get(arg1, parse_pos) == '/') parse_pos = parse_pos + 1;
        day = parse_uint(arg1);
        if (buf_get(arg1, parse_pos) == '-' || buf_get(arg1, parse_pos) == '/') parse_pos = parse_pos + 1;
        year = parse_uint(arg1);
        rc = dos_set_date(year, month, day);
        if (rc == 255) print_dollar("Invalid date\r\n$");
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
    int n;
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
        if (batch_depth != 0) {
            return;
        }
        print_dollar("Enter new time: $");
        buf_set(line, 0, LINE_MAX);
        buf_set(line, 1, 0);
        dos_line_input(line);
        n = buf_get(line, 1);
        if (n <= 0) {
            return;
        }
        buf_set(line, n + 2, 0);
        str_copy(arg1, buf_addr(line, 2), PATH_MAX);
        parse_pos = 0;
        hour = parse_uint(arg1);
        if (buf_get(arg1, parse_pos) == ':') parse_pos = parse_pos + 1;
        min = parse_uint(arg1);
        if (buf_get(arg1, parse_pos) == ':') parse_pos = parse_pos + 1;
        sec = parse_uint(arg1);
        dos_set_time(hour, min, sec);
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
    int drive;
    drive = dos_current_drive();
    skip_spaces();
    if (next_token(arg1, PATH_MAX)) {
        if (str_len(arg1) != 2 || buf_get(arg1, 1) != ':' ||
            buf_get(arg1, 0) < 'A' || buf_get(arg1, 0) > 'Z') {
            print_dollar("VOL [drive:]\r\n$");
            last_errorlevel = 1;
            return;
        }
        drive = buf_get(arg1, 0) - 'A';
    }
    buf_set(pattern, 0, drive + 'A');
    buf_set(pattern, 1, ':');
    buf_set(pattern, 2, '\\');
    buf_set(pattern, 3, '*');
    buf_set(pattern, 4, '.');
    buf_set(pattern, 5, '*');
    buf_set(pattern, 6, 0);
    dos_set_dta(dta);
    print_dollar("Volume in drive $");
    print_char(drive + 'A');
    if (dos_find_first(pattern, 8) == -1) {
        print_dollar(" has no label\r\n$");
        last_errorlevel = 0;
        return;
    }
    print_dollar(" is $");
    print_string(buf_addr(dta, 0x1E));
    print_crlf();
    last_errorlevel = 0;
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

int push_batch_args(void)
{
    int i;
    int c;
    int base;
    if (batch_arg_depth >= BATCH_MAX) {
        return 0;
    }
    base = batch_arg_depth * 160;
    i = 0;
    while (i < 160) {
        c = buf_get(batch_args, i);
        buf_set(batch_arg_frames, base + i, c);
        i = i + 1;
    }
    batch_argc_frames[batch_arg_depth] = batch_argc;
    batch_arg_depth = batch_arg_depth + 1;
    return 1;
}

void pop_batch_args(void)
{
    int i;
    int c;
    int base;
    if (batch_arg_depth <= 0) {
        return;
    }
    batch_arg_depth = batch_arg_depth - 1;
    base = batch_arg_depth * 160;
    i = 0;
    while (i < 160) {
        c = buf_get(batch_arg_frames, base + i);
        buf_set(batch_args, i, c);
        i = i + 1;
    }
    batch_argc = batch_argc_frames[batch_arg_depth];
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
    if (permanent_shell) {
        print_dollar("Primary\r\n$");
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
    int j;
    int idx;
    char out[82];
    i = 0;
    o = 0;
    while (buf_get(cmd, i) != 0 && o < 80) {
        c = buf_get(cmd, i);
        if (c == '%' && buf_get(cmd, i + 1) == '0') {
            i = i + 2;
            j = 0;
            while (buf_get(batch_arg0, j) != 0 && o < 80) {
                buf_set(out, o, buf_get(batch_arg0, j));
                o = o + 1;
                j = j + 1;
            }
        } else if (c == '%' && buf_get(cmd, i + 1) >= '1' && buf_get(cmd, i + 1) <= '9') {
            n = buf_get(cmd, i + 1) - '1';
            i = i + 2;
            j = 0;
            idx = n * 16;
            while (1) {
                c = buf_get(batch_args, idx + j);
                if (c == 0) break;
                if (o >= 80) break;
                buf_set(out, o, c);
                o = o + 1;
                j = j + 1;
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

/* Expand %NAME% from the environment into cmd (after %0–%9). */
void expand_env_percent(void)
{
    int i;
    int o;
    int c;
    int ni;
    int closed;
    int j;
    char out[82];
    char name[32];
    i = 0;
    o = 0;
    while (buf_get(cmd, i) != 0 && o < 80) {
        c = buf_get(cmd, i);
        if (c != '%') {
            buf_set(out, o, c);
            o = o + 1;
            i = i + 1;
        } else {
            i = i + 1;
            c = buf_get(cmd, i);
            if (c == '%') {
                buf_set(out, o, '%');
                o = o + 1;
                i = i + 1;
            } else if (c >= '0' && c <= '9') {
                buf_set(out, o, '%');
                o = o + 1;
                buf_set(out, o, c);
                o = o + 1;
                i = i + 1;
            } else {
                ni = 0;
                closed = 0;
                while (buf_get(cmd, i) != 0 && ni < 31) {
                    c = buf_get(cmd, i);
                    if (c == '%') {
                        closed = 1;
                        i = i + 1;
                        break;
                    }
                    buf_set(name, ni, c);
                    ni = ni + 1;
                    i = i + 1;
                }
                buf_set(name, ni, 0);
                if (closed && env_copy_var(name, env_pathbuf, 128)) {
                    j = 0;
                    while (buf_get(env_pathbuf, j) != 0 && o < 80) {
                        buf_set(out, o, buf_get(env_pathbuf, j));
                        o = o + 1;
                        j = j + 1;
                    }
                } else {
                    buf_set(out, o, '%');
                    o = o + 1;
                    j = 0;
                    while (buf_get(name, j) != 0 && o < 80) {
                        buf_set(out, o, buf_get(name, j));
                        o = o + 1;
                        j = j + 1;
                    }
                    if (closed && o < 80) {
                        buf_set(out, o, '%');
                        o = o + 1;
                    }
                }
            }
        }
    }
    buf_set(out, o, 0);
    str_copy(cmd, out, LINE_MAX);
}

void do_path(void)
{
    int i;
    int c;
    skip_spaces();
    if (peek_byte(cursor) == 0) {
        print_dollar("PATH=$");
        if (env_copy_var("PATH", env_pathbuf, 128)) {
            print_string(env_pathbuf);
        }
        print_crlf();
        return;
    }
    if (peek_byte(cursor) == '=') {
        cursor = cursor + 1;
        skip_spaces();
    }
    i = 0;
    while (peek_byte(cursor) != 0 && i < 126) {
        c = peek_byte(cursor);
        buf_set(env_pathbuf, i, c);
        i = i + 1;
        cursor = cursor + 1;
    }
    buf_set(env_pathbuf, i, 0);
    env_put("PATH", env_pathbuf);
}

void do_ctty(void)
{
    int h;
    if (!next_token(arg1, PATH_MAX)) {
        print_dollar("CTTY device\r\n$");
        last_errorlevel = 1;
        return;
    }
    /* CTTY CON restores prior handles when we previously switched away. */
    if (str_eq(arg1, "CON") && ctty_active) {
        if (ctty_saved0 != 0) {
            dos_force_dup(ctty_saved0, 0);
            dos_close(ctty_saved0);
        }
        if (ctty_saved1 != 0) {
            dos_force_dup(ctty_saved1, 1);
            dos_close(ctty_saved1);
        }
        if (ctty_saved2 != 0) {
            dos_force_dup(ctty_saved2, 2);
            dos_close(ctty_saved2);
        }
        ctty_saved0 = 0;
        ctty_saved1 = 0;
        ctty_saved2 = 0;
        ctty_active = 0;
        last_errorlevel = 0;
        return;
    }
    h = dos_open(arg1, 2);
    if (h == -1) {
        print_dollar("Invalid device\r\n$");
        last_errorlevel = 1;
        return;
    }
    if (!ctty_active) {
        ctty_saved0 = dos_dup(0);
        ctty_saved1 = dos_dup(1);
        ctty_saved2 = dos_dup(2);
        ctty_active = 1;
    }
    dos_force_dup(h, 0);
    dos_force_dup(h, 1);
    dos_force_dup(h, 2);
    if (h > 2) dos_close(h);
    last_errorlevel = 0;
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
    int base;
    if (for_depth >= BATCH_MAX) {
        last_errorlevel = 1;
        print_dollar("FOR nested too deep\r\n$");
        return;
    }
    base = for_depth * 4;
    i = 0;
    while (i < 4) {
        buf_set(for_var_frames, base + i, buf_get(for_var, i));
        i = i + 1;
    }
    base = for_depth * 82;
    i = 0;
    while (i < 82) {
        buf_set(for_body_frames, base + i, buf_get(for_body, i));
        i = i + 1;
    }
    base = for_depth * 64;
    i = 0;
    while (i < 64) {
        buf_set(for_item_frames, base + i, buf_get(for_item, i));
        i = i + 1;
    }
    for_depth = for_depth + 1;
    skip_spaces();
    if (!next_token(arg1, PATH_MAX)) {
        last_errorlevel = 1;
        print_dollar("FOR %v IN (set) DO cmd\r\n$");
    } else {
        i = 0;
        if (buf_get(arg1, 0) == '%') i = 1;
        if (buf_get(arg1, i) == '%') i = i + 1;
        buf_set(for_var, 0, buf_get(arg1, i));
        buf_set(for_var, 1, 0);
        if (!next_token(arg1, PATH_MAX) || !str_eq(arg1, "IN")) {
            last_errorlevel = 1;
            print_dollar("FOR %v IN (set) DO cmd\r\n$");
        } else {
            skip_spaces();
            if (peek_byte(cursor) != '(') {
                last_errorlevel = 1;
                print_dollar("FOR %v IN (set) DO cmd\r\n$");
            } else {
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
                    last_errorlevel = 1;
                    print_dollar("FOR %v IN (set) DO cmd\r\n$");
                } else {
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
                        if (dos_find_first(for_item, 0x10) != -1) {
                            while (1) {
                                for_run_body(buf_addr(dta, 0x1E));
                                if (dos_find_next() == -1) break;
                            }
                        }
                    } else {
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
                }
            }
        }
    }
    for_depth = for_depth - 1;
    base = for_depth * 4;
    i = 0;
    while (i < 4) {
        buf_set(for_var, i, buf_get(for_var_frames, base + i));
        i = i + 1;
    }
    base = for_depth * 82;
    i = 0;
    while (i < 82) {
        buf_set(for_body, i, buf_get(for_body_frames, base + i));
        i = i + 1;
    }
    base = for_depth * 64;
    i = 0;
    while (i < 64) {
        buf_set(for_item, i, buf_get(for_item_frames, base + i));
        i = i + 1;
    }
}


/* Print the value from env for batch %VAR%. */
int print_last_set_value(char *name)
{
    return print_env_value(name);
}

/* ECHO remainder — %VAR% already expanded on the command line. */
void echo_char(int c)
{
    buf_set(copybuf, 0, c);
    dos_write(1, copybuf, 1);
}

void echo_tail(void)
{
    skip_spaces();
    while (peek_byte(cursor) != 0) {
        echo_char(peek_byte(cursor));
        cursor = cursor + 1;
    }
    echo_char(13);
    echo_char(10);
}

void do_batch(char *name)
{
    int h;
    int n;
    int c;
    int slot;
    int base;
    int label;
    int at_cmd;
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
    str_copy(batch_arg0, name, 32);
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
        at_cmd = 0;
        if (buf_get(cmd, 0) == '@') {
            at_cmd = 1;
            str_copy(cmd, buf_addr(cmd, 1), LINE_MAX);
        }
        expand_batch_args();
        expand_env_percent();
        label = 0;
        if (buf_get(cmd, 0) == ':') {
            cursor = buf_addr(cmd, 1);
            next_token(arg1, PATH_MAX);
            label = 1;
            if (goto_active && str_eq(arg1, goto_name)) goto_active = 0;
        }
        if (!goto_active && !label && buf_get(cmd, 0) != 0) {
            if (echo_on && !at_cmd) {
                print_string(cmd);
                print_crlf();
            }
            dispatch();
        }
        if (c <= 0) {
            break;
        }
    }
    batch_depth = batch_depth - 1;
    dos_close(h);
    if (batch_depth > 0) {
        str_copy(batch_arg0, buf_addr(batch_names, (batch_depth - 1) * 32), 32);
    } else {
        buf_set(batch_arg0, 0, 0);
    }
}


/* Split remainder at ELSE and run then or else branch. */
void if_run_tail(int take_then)
{
    int i;
    int o;
    int c;
    int has_else;
    int else_at;
    skip_spaces();
    i = 0;
    o = 0;
    has_else = 0;
    else_at = 0;
    while (peek_byte(cursor) != 0 && o < 80) {
        c = peek_byte(cursor);
        if ((c == 'E' || c == 'e') && (o == 0 || buf_get(if_then, o - 1) == ' ')) {
            if ((peek_byte(cursor + 1) == 'L' || peek_byte(cursor + 1) == 'l')
                && (peek_byte(cursor + 2) == 'S' || peek_byte(cursor + 2) == 's')
                && (peek_byte(cursor + 3) == 'E' || peek_byte(cursor + 3) == 'e')) {
                c = peek_byte(cursor + 4);
                if (c == 0 || c == ' ' || c == 9) {
                    has_else = 1;
                    else_at = o;
                    cursor = cursor + 4;
                    break;
                }
            }
        }
        buf_set(if_then, o, peek_byte(cursor));
        o = o + 1;
        cursor = cursor + 1;
    }
    buf_set(if_then, o, 0);
    if (has_else) {
        while (else_at > 0 && buf_get(if_then, else_at - 1) == ' ') {
            else_at = else_at - 1;
        }
        buf_set(if_then, else_at, 0);
        skip_spaces();
        o = 0;
        while (peek_byte(cursor) != 0 && o < 80) {
            buf_set(if_else, o, peek_byte(cursor));
            o = o + 1;
            cursor = cursor + 1;
        }
        buf_set(if_else, o, 0);
    } else {
        buf_set(if_else, 0, 0);
    }
    if (take_then) {
        if (buf_get(if_then, 0) != 0) {
            str_copy(cmd, if_then, LINE_MAX);
            dispatch();
        }
    } else {
        if (buf_get(if_else, 0) != 0) {
            str_copy(cmd, if_else, LINE_MAX);
            dispatch();
        }
    }
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
        if_run_tail(last_errorlevel >= target);
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
            if_run_tail(!exists);
        } else {
            if_run_tail(exists);
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
        if_run_tail(!eq);
    } else {
        if_run_tail(eq);
    }
}

void restore_redirection(void)
{
    if (saved_stdout != -1) {
        dos_force_dup(saved_stdout, 1);
        dos_close(saved_stdout);
        saved_stdout = -1;
    }
    if (redir_out_handle != -1) {
        dos_close(redir_out_handle);
        redir_out_handle = -1;
    }
    if (saved_stdin != -1) {
        dos_force_dup(saved_stdin, 0);
        dos_close(saved_stdin);
        saved_stdin = -1;
    }
    if (redir_in_handle != -1) {
        dos_close(redir_in_handle);
        redir_in_handle = -1;
    }
}

int setup_redirection(void)
{
    int h;
    if (redir_in_kind != 0) {
        h = dos_open(redir_in_name, 0);
        if (h == -1) return 0;
        redir_in_handle = h;
        saved_stdin = dos_dup(0);
        if (saved_stdin == -1) {
            restore_redirection();
            return 0;
        }
        if (dos_force_dup(h, 0) == -1) {
            restore_redirection();
            return 0;
        }
    }
    if (redir_out_kind != 0) {
        if (redir_out_kind == 2) {
            h = dos_open(redir_out_name, 1);
            if (h != -1) dos_seek_end(h);
        } else {
            h = -1;
        }
        if (h == -1) h = dos_create(redir_out_name, 0);
        if (h == -1) {
            restore_redirection();
            return 0;
        }
        redir_out_handle = h;
        saved_stdout = dos_dup(1);
        if (saved_stdout == -1) {
            restore_redirection();
            return 0;
        }
        if (dos_force_dup(h, 1) == -1) {
            restore_redirection();
            return 0;
        }
    }
    return 1;
}

void parse_redirection(void)
{
    int i;
    int o;
    int c;
    int n;
    int kind;
    redir_in_kind = 0;
    redir_out_kind = 0;
    redir_in_handle = -1;
    redir_out_handle = -1;
    saved_stdin = -1;
    saved_stdout = -1;
    buf_set(redir_in_name, 0, 0);
    buf_set(redir_out_name, 0, 0);
    i = 0;
    o = 0;
    while (buf_get(cmd, i) != 0) {
        c = buf_get(cmd, i);
        if (c == '>' || c == '<') {
            if (c == '>') kind = 1;
            else kind = 3;
            i = i + 1;
            if (c == '>' && buf_get(cmd, i) == '>') {
                kind = 2;
                i = i + 1;
            }
            while (buf_get(cmd, i) == ' ' || buf_get(cmd, i) == 9) i = i + 1;
            n = 0;
            while (buf_get(cmd, i) != 0
                && buf_get(cmd, i) != ' '
                && buf_get(cmd, i) != 9
                && buf_get(cmd, i) != '<'
                && buf_get(cmd, i) != '>') {
                if (n < PATH_MAX - 1) {
                    if (c == '<') buf_set(redir_in_name, n, buf_get(cmd, i));
                    else buf_set(redir_out_name, n, buf_get(cmd, i));
                    n = n + 1;
                }
                i = i + 1;
            }
            if (c == '<') {
                buf_set(redir_in_name, n, 0);
                redir_in_kind = kind;
            } else {
                buf_set(redir_out_name, n, 0);
                redir_out_kind = kind;
            }
        } else {
            buf_set(cmd, o, c);
            o = o + 1;
            i = i + 1;
        }
    }
    while (o > 0 && (buf_get(cmd, o - 1) == ' ' || buf_get(cmd, o - 1) == 9)) {
        o = o - 1;
    }
    buf_set(cmd, o, 0);
}

int parse_startup_tail(void)
{
    int n;
    int i;
    int o;
    int c;
    int v;
    n = peek_byte(0x80);
    i = 0;
    permanent_shell = 0;
    env_paras = 32;
    while (i < n) {
        while (i < n && (peek_byte(0x81 + i) == ' ' || peek_byte(0x81 + i) == 9)) {
            i = i + 1;
        }
        if (i >= n) break;
        c = peek_byte(0x81 + i);
        if (c != '/' && c != '-') break;
        i = i + 1;
        c = toupper_ch(peek_byte(0x81 + i));
        i = i + 1;
        if (c == 'C') {
            while (i < n && (peek_byte(0x81 + i) == ' ' || peek_byte(0x81 + i) == 9)) {
                i = i + 1;
            }
            o = 0;
            while (i < n && o < LINE_MAX) {
                buf_set(cmd, o, peek_byte(0x81 + i));
                i = i + 1;
                o = o + 1;
            }
            buf_set(cmd, o, 0);
            return 1;
        }
        if (c == 'P') {
            permanent_shell = 1;
        } else if (c == 'E') {
            /* /E:nnnn — environment size in bytes → paragraphs */
            if (i < n && peek_byte(0x81 + i) == ':') i = i + 1;
            v = 0;
            while (i < n) {
                c = peek_byte(0x81 + i);
                if (c < '0' || c > '9') break;
                v = v * 10 + (c - '0');
                i = i + 1;
            }
            if (v < 160) v = 160;
            if (v > 32768) v = 32768;
            env_paras = (v + 15) / 16;
        }
        while (i < n && peek_byte(0x81 + i) != ' ' && peek_byte(0x81 + i) != 9) {
            i = i + 1;
        }
    }
    return 0;
}

void dispatch_plain(void)
{
    int h;
    int i;
    int c;
    int drive;
    int echo_save;
    cursor = buf_addr(cmd, 0);
    if (!next_token(prog, PATH_MAX)) return;
    if (is_drive_spec(prog)) {
        dos_set_drive(buf_get(prog, 0) - 'A');
        return;
    }
    if (str_eq(prog, "DIR")) { do_dir(); return; }
    if (str_eq(prog, "TYPE")) { do_type(); return; }
    /* COPY falls through to PATH exec of BIN\COPY.COM */
    if (str_eq(prog, "DEL") || str_eq(prog, "ERASE")) { do_del(); return; }
    if (str_eq(prog, "PATH")) { do_path(); return; }
    if (str_eq(prog, "CLS")) { clear_screen(); return; }
    if (str_eq(prog, "CD") || str_eq(prog, "CHDIR")) {
        if (next_token(arg1, PATH_MAX)) {
            if (dos_chdir(arg1) == -1) {
                print_dollar("Invalid directory\r\n$");
                last_errorlevel = 1;
            } else {
                last_errorlevel = 0;
            }
        } else {
            drive = dos_current_drive();
            print_char(drive + 'A');
            print_char(':');
            get_cwd(cwd);
            print_string(cwd);
            print_crlf();
            last_errorlevel = 0;
        }
        return;
    }
    if (str_eq(prog, "MD") || str_eq(prog, "MKDIR")) {
        if (!next_token(arg1, PATH_MAX) || dos_mkdir(arg1) == -1) {
            print_dollar("Unable to create directory\r\n$");
            last_errorlevel = 1;
        } else {
            last_errorlevel = 0;
        }
        return;
    }
    if (str_eq(prog, "RD") || str_eq(prog, "RMDIR")) {
        if (!next_token(arg1, PATH_MAX) || dos_rmdir(arg1) == -1) {
            print_dollar("Invalid path, not directory,\r\nor directory not empty\r\n$");
            last_errorlevel = 1;
        } else {
            last_errorlevel = 0;
        }
        return;
    }
    if (str_eq(prog, "REN") || str_eq(prog, "RENAME")) {
        if (!next_token(arg1, PATH_MAX) || !next_token(arg2, PATH_MAX) || dos_rename(arg1, arg2) == -1) {
            print_dollar("RENAME old new\r\n$");
            last_errorlevel = 1;
        } else {
            last_errorlevel = 0;
        }
        return;
    }
    if (str_eq(prog, "ECHO") || is_echo_punct(prog)) {
        if (is_echo_punct(prog)) {
            print_crlf();
            return;
        }
        skip_spaces();
        echo_save = cursor;
        if (next_token(arg1, PATH_MAX) && peek_byte(cursor) == 0) {
            if (str_eq(arg1, "ON")) {
                echo_on = 1;
                return;
            }
            if (str_eq(arg1, "OFF")) {
                echo_on = 0;
                return;
            }
        }
        cursor = echo_save;
        echo_tail();
        return;
    }
    if (str_eq(prog, "PAUSE")) { print_dollar("Press any key to continue . . .$"); read_key(); print_crlf(); return; }
    if (str_eq(prog, "VER")) { h = dos_version(); print_dollar("rmDOS DOS $"); print_num(h & 255); print_char('.'); print_num(h >> 8); print_crlf(); return; }
    if (str_eq(prog, "SET")) { do_set(); return; }
    if (str_eq(prog, "REM")) { return; }
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
            if (push_batch_args()) {
                collect_batch_args();
                do_batch(arg1);
                pop_batch_args();
            }
        }
        return;
    }
    if (str_eq(prog, "GOTO")) {
        if (next_token(goto_name, PATH_MAX) && batch_depth > 0) {
            dos_seek_start(batch_handles[batch_depth - 1]);
            goto_active = 1;
        }
        return;
    }
    if (str_has(prog, '.') && str_eq(buf_addr(prog, str_len(prog) - 4), ".BAT")) {
        if (push_batch_args()) {
            collect_batch_args();
            do_batch(prog);
            pop_batch_args();
        }
        return;
    }
    str_copy(prog_base, prog, PATH_MAX);
    make_exec_tail();
    if (!try_exec_name()) {
        str_copy(prog, prog_base, PATH_MAX);
        if (try_batch_name()) return;
        if (!str_has(prog_base, '\\') && !str_has(prog_base, '/')) {
            /* Walk PATH= from environment */
            if (env_copy_var("PATH", env_pathbuf, 128)) {
                i = 0;
                while (1) {
                    c = 0;
                    while (buf_get(env_pathbuf, i) != 0 && buf_get(env_pathbuf, i) != ';') {
                        buf_set(env_join, c, buf_get(env_pathbuf, i));
                        c = c + 1;
                        i = i + 1;
                        if (c >= PATH_MAX - 2) break;
                    }
                    buf_set(env_join, c, 0);
                    if (c > 0) {
                        if (buf_get(env_join, c - 1) != '\\') {
                            buf_set(env_join, c, '\\');
                            c = c + 1;
                            buf_set(env_join, c, 0);
                        }
                        str_copy(buf_addr(env_join, c), prog_base, PATH_MAX - c);
                        str_copy(prog, env_join, PATH_MAX);
                        if (try_exec_name()) return;
                        if (try_batch_name()) return;
                    }
                    if (buf_get(env_pathbuf, i) == 0) break;
                    i = i + 1;
                }
            }
            /* Fallback if PATH missing/unreadable after a child */
            str_copy(prog, "A:\\BIN\\", PATH_MAX);
            str_copy(buf_addr(prog, 7), prog_base, PATH_MAX - 7);
            if (try_exec_name()) return;
            if (try_batch_name()) return;
        }
        last_errorlevel = 1;
        print_dollar("Bad command\r\n$");
    }
}

int make_pipe_temp(void)
{
    int tries;
    int n;
    int h;
    tries = 0;
    while (tries < 10000) {
        n = pipe_seq;
        pipe_seq = pipe_seq + 1;
        if (pipe_seq >= 10000) pipe_seq = 0;
        buf_set(pipe_tmp, 0, dos_current_drive() + 'A');
        buf_set(pipe_tmp, 1, ':');
        buf_set(pipe_tmp, 2, '\\');
        buf_set(pipe_tmp, 3, 'P');
        buf_set(pipe_tmp, 4, 'I');
        buf_set(pipe_tmp, 5, 'P');
        buf_set(pipe_tmp, 6, 'E');
        buf_set(pipe_tmp, 7, ((n / 1000) % 10) + '0');
        buf_set(pipe_tmp, 8, ((n / 100) % 10) + '0');
        buf_set(pipe_tmp, 9, ((n / 10) % 10) + '0');
        buf_set(pipe_tmp, 10, (n % 10) + '0');
        buf_set(pipe_tmp, 11, '.');
        buf_set(pipe_tmp, 12, '$');
        buf_set(pipe_tmp, 13, '$');
        buf_set(pipe_tmp, 14, 0);
        h = dos_create_new(pipe_tmp, 0);
        if (h != -1) return h;
        tries = tries + 1;
    }
    return -1;
}

int run_pipe(void)
{
    int i;
    int h;
    int input_active;
    int pipe_saved_in;
    int pipe_saved_out;
    i = 0;
    while (buf_get(cmd, i) != 0 && buf_get(cmd, i) != '|') i = i + 1;
    if (buf_get(cmd, i) == 0) return 0;
    input_active = 0;
    pipe_saved_in = -1;
    while (1) {
        i = 0;
        while (buf_get(cmd, i) != 0 && buf_get(cmd, i) != '|') i = i + 1;
        if (buf_get(cmd, i) == 0) {
            parse_redirection();
            if (setup_redirection()) {
                dispatch_plain();
                restore_redirection();
            }
            if (input_active) {
                dos_force_dup(pipe_saved_in, 0);
                dos_close(pipe_saved_in);
                dos_delete(pipe_input);
            }
            return 1;
        }
        buf_set(cmd, i, 0);
        str_copy(pipe_rhs, buf_addr(cmd, i + 1), LINE_MAX);
        h = make_pipe_temp();
        if (h == -1) {
            if (input_active) {
                dos_force_dup(pipe_saved_in, 0);
                dos_close(pipe_saved_in);
                dos_delete(pipe_input);
            }
            return 1;
        }
        pipe_saved_out = dos_dup(1);
        if (pipe_saved_out == -1) {
            dos_close(h);
            dos_delete(pipe_tmp);
            if (input_active) {
                dos_force_dup(pipe_saved_in, 0);
                dos_close(pipe_saved_in);
                dos_delete(pipe_input);
            }
            return 1;
        }
        dos_force_dup(h, 1);
        parse_redirection();
        if (setup_redirection()) {
            dispatch_plain();
            restore_redirection();
        }
        dos_force_dup(pipe_saved_out, 1);
        dos_close(pipe_saved_out);
        dos_close(h);
        if (input_active) {
            dos_force_dup(pipe_saved_in, 0);
            dos_close(pipe_saved_in);
            dos_delete(pipe_input);
            input_active = 0;
        }
        h = dos_open(pipe_tmp, 0);
        if (h == -1) {
            dos_delete(pipe_tmp);
            return 1;
        }
        pipe_saved_in = dos_dup(0);
        if (pipe_saved_in == -1) {
            dos_close(h);
            dos_delete(pipe_tmp);
            return 1;
        }
        dos_force_dup(h, 0);
        dos_close(h);
        str_copy(pipe_input, pipe_tmp, 16);
        input_active = 1;
        str_copy(cmd, pipe_rhs, LINE_MAX);
    }
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
            expand_batch_args();
            expand_env_percent();
            dispatch();
        }
    }
}

int main(void)
{
    reload_ds();
    echo_on = 1;
    if (parse_startup_tail()) {
        expand_batch_args();
        expand_env_percent();
        dispatch();
        /* Returning reaches the COM startup's AH=4Ch with AX as the code. */
        return last_errorlevel;
    }
    do_batch("AUTOEXEC.BAT");
    main_loop();
    return 0;
}
