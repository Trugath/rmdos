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
static char copybuf[128] = { 0 };
static char pipe_rhs[82] = { 0 };
static char redir_name[64] = { 0 };
static char exec_tail[128] = { 0 };
static char exec_pb[14] = { 0 };
static char fcb1[16] = { 0 };
static char fcb2[16] = { 0 };
static char batch_names[256] = { 0 };
static char batch_args[1280] = { 0 };
static char goto_name[64] = { 0 };
static char last_set[80] = { 0 };
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

/* DOS calls which are not shared by the other Small-C tools. */
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
    int drive;
    drive = dos_current_drive();
    print_char(drive + 'A');
    print_dollar(":$");
    get_cwd(cwd);
    print_string(cwd);
    print_dollar("> $");
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
    base = slot * 64;
    str_copy(buf_addr(batch_names, base), name, 64);
    batch_depth = batch_depth + 1;
    n = 0;
    while (1) {
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
    exists = 0;
    if (str_eq(arg1, "NOT")) {
        exists = 1;
        next_token(arg1, PATH_MAX);
    }
    if (!str_eq(arg1, "EXIST")) {
        return;
    }
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
    if (str_eq(prog, "IF")) { do_if(); return; }
    if (str_eq(prog, "CALL")) { if (next_token(arg1, PATH_MAX)) do_batch(arg1); return; }
    if (str_eq(prog, "GOTO")) {
        if (next_token(goto_name, PATH_MAX)) goto_active = 1;
        return;
    }
    if (str_has(prog, '.') && str_eq(buf_addr(prog, str_len(prog) - 4), ".BAT")) { do_batch(prog); return; }
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
