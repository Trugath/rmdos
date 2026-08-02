/* ATTRIB.COM — [+R|-R] [+A|-A] [+S|-S] [+H|-H] [/S] file */
#include "dos.h"

#define PATH_MAX 80
#define MAX_DEPTH 8

static char pattern[PATH_MAX];
static char file_pat[16];
static char full_path[PATH_MAX];
static char search_pattern[PATH_MAX];
static char entry_name[16];
static char dirs[640];
static char dta_frames[1024];
static char sep[3] = " $";
static char crlf[4] = "\r\n$";
static char msg_u[52] = "ATTRIB [+R|-R] [+A|-A] [+S|-S] [+H|-H] [/S] file\r\n$";
static char msg_e[17] = "ATTRIB failed\r\n$";

static int mask;
static int bits;
static int set_mode;
static int opt_s;
static int matched;

static int str_length(char *s)
{
    int i;
    i = 0;
    while (buf_get(s, i) != 0) i = i + 1;
    return i;
}

static int join_path(char *out, char *base, char *name)
{
    int i;
    int j;
    int c;
    i = 0;
    while (buf_get(base, i) != 0) {
        if (i >= PATH_MAX - 1) return 0;
        buf_set(out, i, buf_get(base, i));
        i = i + 1;
    }
    if (i > 0) {
        c = buf_get(out, i - 1);
        if (c != '\\' && c != '/' && c != ':') {
            if (i >= PATH_MAX - 1) return 0;
            buf_set(out, i, '\\');
            i = i + 1;
        }
    }
    j = 0;
    while (buf_get(name, j) != 0) {
        if (i >= PATH_MAX - 1) return 0;
        buf_set(out, i, buf_get(name, j));
        i = i + 1;
        j = j + 1;
    }
    buf_set(out, i, 0);
    return 1;
}

static void copy_entry_name(char *dta)
{
    int i;
    int c;
    i = 0;
    while (i < 15) {
        c = buf_get(dta, 0x1E + i);
        buf_set(entry_name, i, c);
        if (c == 0) return;
        i = i + 1;
    }
    buf_set(entry_name, 15, 0);
}

static int is_dot_directory(void)
{
    if (buf_get(entry_name, 0) != '.') return 0;
    if (buf_get(entry_name, 1) == 0) return 1;
    if (buf_get(entry_name, 1) == '.' && buf_get(entry_name, 2) == 0) return 1;
    return 0;
}

static void show_attrs(int attr)
{
    if (attr & 1) print_char('R'); else print_char('-');
    if (attr & 0x20) print_char('A'); else print_char('-');
    if (attr & 4) print_char('S'); else print_char('-');
    if (attr & 2) print_char('H'); else print_char('-');
}

static int process_files(int depth)
{
    int dta;
    int dir;
    int attr;
    dta = buf_addr(dta_frames, depth * 128);
    dir = buf_addr(dirs, depth * PATH_MAX);
    if (!join_path(search_pattern, dir, file_pat)) return 0;
    dos_set_dta(dta);
    if (dos_find_first(search_pattern, 0x27) == -1) return 1;
    while (1) {
        attr = buf_get(dta, 0x15);
        if (!(attr & 0x10)) {
            copy_entry_name(dta);
            if (!join_path(full_path, dir, entry_name)) return 0;
            if (set_mode) {
                attr = (attr & 0x3F & ~mask) | bits;
                if (dos_chmod(full_path, 1, attr) == -1) return 0;
            }
            show_attrs(attr);
            print_dollar(sep);
            print_string(entry_name);
            print_dollar(crlf);
            matched = matched + 1;
        }
        if (dos_find_next() == -1) break;
    }
    return 1;
}

static int walk_tree(int depth)
{
    int dta;
    int dir;
    int child;
    int attr;
    if (!process_files(depth)) return 0;
    if (!opt_s) return 1;
    if (depth + 1 >= MAX_DEPTH) return 1;
    dta = buf_addr(dta_frames, depth * 128);
    dir = buf_addr(dirs, depth * PATH_MAX);
    if (!join_path(search_pattern, dir, "*.*")) return 0;
    dos_set_dta(dta);
    if (dos_find_first(search_pattern, 0x37) == -1) return 1;
    while (1) {
        attr = buf_get(dta, 0x15);
        if (attr & 0x10) {
            copy_entry_name(dta);
            if (!is_dot_directory()) {
                child = buf_addr(dirs, (depth + 1) * PATH_MAX);
                if (!join_path(child, dir, entry_name)) return 0;
                if (!walk_tree(depth + 1)) return 0;
                dos_set_dta(dta);
            }
        }
        if (dos_find_next() == -1) break;
    }
    return 1;
}

static int prepare_pattern(void)
{
    int i;
    int last;
    int c;
    int o;
    int root;
    root = buf_addr(dirs, 0);
    i = 0;
    last = -1;
    while (1) {
        c = buf_get(pattern, i);
        if (c == 0) break;
        if (c == '\\' || c == '/' || c == ':') last = i;
        i = i + 1;
    }
    o = 0;
    i = last + 1;
    while (buf_get(pattern, i) != 0) {
        buf_set(file_pat, o, buf_get(pattern, i));
        o = o + 1;
        i = i + 1;
    }
    buf_set(file_pat, o, 0);
    if (o == 0) {
        buf_set(file_pat, 0, '*');
        buf_set(file_pat, 1, '.');
        buf_set(file_pat, 2, '*');
        buf_set(file_pat, 3, 0);
    }
    if (last < 0) {
        buf_set(root, 0, 0);
        return 1;
    }
    o = 0;
    while (o <= last) {
        buf_set(root, o, buf_get(pattern, o));
        o = o + 1;
    }
    /* drop trailing separator except drive root */
    if (o > 1 && (buf_get(root, o - 1) == '\\' || buf_get(root, o - 1) == '/')) {
        if (!(o == 3 && buf_get(root, 1) == ':')) {
            o = o - 1;
            buf_set(root, o, 0);
        }
    }
    return 1;
}

int main(void)
{
    int c;
    int cl;

    mask = 0;
    bits = 0;
    set_mode = 0;
    opt_s = 0;
    matched = 0;
    buf_set(pattern, 0, 0);
    args_init();
    while (args_skip()) {
        c = peek_byte(arg_ptr);
        if (c == '+' || c == '-') {
            arg_ptr = arg_ptr + 1;
            cl = toupper_ch(peek_byte(arg_ptr));
            arg_ptr = arg_ptr + 1;
            if (cl == 'R') cl = 1;
            else if (cl == 'A') cl = 0x20;
            else if (cl == 'S') cl = 4;
            else if (cl == 'H') cl = 2;
            else cl = 0;
            if (cl != 0) {
                mask = mask | cl;
                bits = bits & ~cl;
                if (c == '+') bits = bits | cl;
                set_mode = 1;
            }
        } else if (c == '/' || c == '-') {
            arg_ptr = arg_ptr + 1;
            cl = toupper_ch(peek_byte(arg_ptr));
            if (cl == 'S') {
                opt_s = 1;
                arg_ptr = arg_ptr + 1;
            } else {
                print_dollar(msg_u);
                return 1;
            }
        } else {
            args_token(pattern, PATH_MAX);
        }
    }
    if (buf_get(pattern, 0) == 0) {
        print_dollar(msg_u);
        return 1;
    }
    if (!prepare_pattern()) {
        print_dollar(msg_e);
        return 1;
    }
    if (!walk_tree(0) || matched == 0) {
        print_dollar(msg_e);
        return 1;
    }
    return 0;
}
