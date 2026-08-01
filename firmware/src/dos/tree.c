/* TREE.COM — list first-level subdirectories under a path. */
#include "dos.h"

#define PATH_MAX 64
#define DTA_SIZE 128

static char path[PATH_MAX];
static char pattern[PATH_MAX];
static char dta[DTA_SIZE];
static char name[16];
static char msg_none[20] = "No subfolders\r\n$";
static int found_any;

static void copy_name(void)
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

int main(void)
{
    int i;
    int attr;
    int c;

    args_init();
    if (!args_token(path, PATH_MAX)) {
        /* Current directory: match root entries. */
        buf_set(pattern, 0, '*');
        buf_set(pattern, 1, '.');
        buf_set(pattern, 2, '*');
        buf_set(pattern, 3, 0);
        buf_set(path, 0, 'A');
        buf_set(path, 1, ':');
        buf_set(path, 2, '\\');
        buf_set(path, 3, 0);
    } else {
        i = 0;
        while (buf_get(path, i) != 0 && i < PATH_MAX - 5) {
            buf_set(pattern, i, buf_get(path, i));
            i = i + 1;
        }
        c = 0;
        if (i > 0) c = buf_get(pattern, i - 1);
        if (c != '\\' && c != ':' && i > 0) {
            buf_set(pattern, i, '\\');
            i = i + 1;
        }
        buf_set(pattern, i, '*');
        buf_set(pattern, i + 1, '.');
        buf_set(pattern, i + 2, '*');
        buf_set(pattern, i + 3, 0);
    }

    found_any = 0;
    print_dollar("Directory PATH listing\r\n$");
    print_string(path);
    print_dollar("\r\n$");
    dos_set_dta(dta);
    if (dos_find_first(pattern, 0x10) == -1) {
        print_dollar(msg_none);
        return 0;
    }
    while (1) {
        attr = buf_get(dta, 0x15);
        if (attr & 0x10) {
            copy_name();
            if (!is_dot_dir()) {
                found_any = 1;
                print_dollar("+---$");
                print_string(name);
                print_dollar("\r\n$");
            }
        }
        if (dos_find_next() == -1) break;
    }
    if (!found_any) {
        print_dollar(msg_none);
    }
    return 0;
}
