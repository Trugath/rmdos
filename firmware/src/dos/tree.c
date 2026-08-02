/* TREE.COM — recursive directory tree; /F lists files. */
#include "dos.h"

#define PATH_MAX 64
#define MAX_DEPTH 8
#define DTA_SIZE 128

static char path[PATH_MAX];
static char pattern[PATH_MAX];
static char name[16];
static char dirs[512];
static char dta_frames[1024];
static char msg_none[20] = "No subfolders\r\n$";
static int opt_f;
static int found_any;

static void copy_name(char *dta)
{
    int i;
    int c;
    i = 0;
    while (i < 13) {
        c = buf_get(dta, 0x1E + i);
        buf_set(name, i, c);
        if (c == 0) break;
        i = i + 1;
    }
    buf_set(name, i, 0);
}

static int is_dot_dir(void)
{
    if (buf_get(name, 0) == '.' && buf_get(name, 1) == 0) return 1;
    if (buf_get(name, 0) == '.' && buf_get(name, 1) == '.' && buf_get(name, 2) == 0) return 1;
    return 0;
}

static int join_path(char *out, char *base, char *leaf)
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
    while (buf_get(leaf, j) != 0) {
        if (i >= PATH_MAX - 1) return 0;
        buf_set(out, i, buf_get(leaf, j));
        i = i + 1;
        j = j + 1;
    }
    buf_set(out, i, 0);
    return 1;
}

static void print_indent(int depth)
{
    int i;
    i = 0;
    while (i < depth) {
        print_dollar("|   $");
        i = i + 1;
    }
}

static void list_files(int depth)
{
    int dta;
    int dir;
    int attr;
    dta = buf_addr(dta_frames, depth * DTA_SIZE);
    dir = buf_addr(dirs, depth * PATH_MAX);
    if (!join_path(pattern, dir, "*.*")) return;
    dos_set_dta(dta);
    if (dos_find_first(pattern, 0x27) == -1) return;
    while (1) {
        attr = buf_get(dta, 0x15);
        if (!(attr & 0x10)) {
            copy_name(dta);
            print_indent(depth);
            print_dollar("    $");
            print_string(name);
            print_dollar("\r\n$");
        }
        if (dos_find_next() == -1) break;
    }
}

static void walk_tree(int depth)
{
    int dta;
    int dir;
    int child;
    int attr;
    if (opt_f) list_files(depth);
    if (depth + 1 >= MAX_DEPTH) return;
    dta = buf_addr(dta_frames, depth * DTA_SIZE);
    dir = buf_addr(dirs, depth * PATH_MAX);
    if (!join_path(pattern, dir, "*.*")) return;
    dos_set_dta(dta);
    if (dos_find_first(pattern, 0x10) == -1) return;
    while (1) {
        attr = buf_get(dta, 0x15);
        if (attr & 0x10) {
            copy_name(dta);
            if (!is_dot_dir()) {
                found_any = 1;
                print_indent(depth);
                print_dollar("+---$");
                print_string(name);
                print_dollar("\r\n$");
                child = buf_addr(dirs, (depth + 1) * PATH_MAX);
                if (join_path(child, dir, name)) {
                    walk_tree(depth + 1);
                }
                dos_set_dta(dta);
            }
        }
        if (dos_find_next() == -1) break;
    }
}

int main(void)
{
    int i;
    int c;
    int root;

    opt_f = 0;
    buf_set(path, 0, 0);
    args_init();
    while (args_skip()) {
        c = peek_byte(arg_ptr);
        if (c == '/' || c == '-') {
            arg_ptr = arg_ptr + 1;
            c = toupper_ch(peek_byte(arg_ptr));
            if (c == 'F') {
                opt_f = 1;
                arg_ptr = arg_ptr + 1;
            }
        } else {
            args_token(path, PATH_MAX);
        }
    }

    root = buf_addr(dirs, 0);
    if (buf_get(path, 0) == 0) {
        buf_set(path, 0, 'A');
        buf_set(path, 1, ':');
        buf_set(path, 2, '\\');
        buf_set(path, 3, 0);
    }
    i = 0;
    while (buf_get(path, i) != 0 && i < PATH_MAX - 1) {
        buf_set(root, i, buf_get(path, i));
        i = i + 1;
    }
    buf_set(root, i, 0);
    c = 0;
    if (i > 0) c = buf_get(root, i - 1);
    if (c == '\\' || c == '/') {
        if (!(i == 3 && buf_get(root, 1) == ':')) {
            i = i - 1;
            buf_set(root, i, 0);
        }
    }

    found_any = 0;
    print_dollar("Directory PATH listing\r\n$");
    print_string(path);
    print_dollar("\r\n$");
    walk_tree(0);
    if (!found_any && !opt_f) {
        print_dollar(msg_none);
    }
    return 0;
}
