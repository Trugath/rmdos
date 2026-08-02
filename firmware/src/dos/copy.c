/* COPY.COM — COPY [/V][/A][/B] src[+src] dst. */
#include "dos.h"

#define PATH_MAX 64

static char src[PATH_MAX];
static char dst[PATH_MAX];
static char src_part[PATH_MAX];
static char src_path[PATH_MAX];
static char dst_path[PATH_MAX];
static char match_name[16];
static char dta[128];
static char buf[512];
static char msg_ok[12] = "COPYV OK\r\n$";
static char msg_copied[10] = "copied\r\n$";
static char msg_err[15] = "COPY failed\r\n$";
static char msg_u[35] = "COPY [/V][/A][/B] src[+src] dst\r\n$";
static int opt_v;
static int opt_a;
static int opt_b;
static int verify_saved;
static int destination_is_dir;
static int source_has_plus;
static int source_has_wild;
static int output_handle;
static int copied_count;

static void str_copy_limit(char *out, char *in, int max)
{
    int i;
    int c;
    i = 0;
    while (i < max - 1) {
        c = buf_get(in, i);
        buf_set(out, i, c);
        if (c == 0) return;
        i = i + 1;
    }
    buf_set(out, i, 0);
}

static int str_length(char *s)
{
    int i;
    i = 0;
    while (buf_get(s, i) != 0) i = i + 1;
    return i;
}

static int has_wildcard(char *s)
{
    int i;
    int c;
    i = 0;
    while (1) {
        c = buf_get(s, i);
        if (c == 0) return 0;
        if (c == '*' || c == '?') return 1;
        i = i + 1;
    }
}

static int has_plus(char *s)
{
    int i;
    i = 0;
    while (buf_get(s, i) != 0) {
        if (buf_get(s, i) == '+') return 1;
        i = i + 1;
    }
    return 0;
}

static int eat_switch(void)
{
    int c;
    int seen;
    seen = 0;
    while (1) {
        c = peek_byte(arg_ptr);
        if (c == ' ' || c == 9 || c == 13 || c == 0) break;
        if (c == '/' || c == '-') {
            arg_ptr = arg_ptr + 1;
        } else {
            c = toupper_ch(c);
            if (c == 'V') {
                opt_v = 1;
            } else if (c == 'A') {
                opt_a = 1;
                opt_b = 0;
            } else if (c == 'B') {
                opt_b = 1;
                opt_a = 0;
            } else {
                return 0;
            }
            seen = 1;
            arg_ptr = arg_ptr + 1;
        }
    }
    return seen;
}

static void verify_begin(void)
{
    asm("mov ah, 0x54");
    asm("int 0x21");
    asm("mov ah, 0");
    asm("mov [verify_saved], ax");
    asm("mov ax, 0x2E01");
    asm("int 0x21");
    reload_ds();
}

static void verify_end(void)
{
    asm("mov al, byte ptr [verify_saved]");
    asm("mov ah, 0x2E");
    asm("int 0x21");
    reload_ds();
}

static int copy_basename(char *path, char *name)
{
    int i;
    int last;
    int c;
    int o;
    i = 0;
    last = 0;
    while (1) {
        c = buf_get(path, i);
        if (c == 0) break;
        if (c == '\\' || c == '/' || c == ':') last = i + 1;
        i = i + 1;
    }
    o = 0;
    while (last < i && o < 15) {
        buf_set(name, o, buf_get(path, last));
        o = o + 1;
        last = last + 1;
    }
    buf_set(name, o, 0);
    if (o == 0) return 0;
    return 1;
}

static int build_source_path(char *pattern, char *name)
{
    int i;
    int prefix;
    int c;
    int j;
    i = 0;
    prefix = 0;
    while (1) {
        c = buf_get(pattern, i);
        if (c == 0) break;
        if (c == '\\' || c == '/' || c == ':') prefix = i + 1;
        i = i + 1;
    }
    if (prefix + str_length(name) >= PATH_MAX) return 0;
    i = 0;
    while (i < prefix) {
        buf_set(src_path, i, buf_get(pattern, i));
        i = i + 1;
    }
    j = 0;
    while (1) {
        c = buf_get(name, j);
        buf_set(src_path, i, c);
        if (c == 0) break;
        i = i + 1;
        j = j + 1;
    }
    return 1;
}

static int build_destination(char *name)
{
    int i;
    int j;
    int c;
    str_copy_limit(dst_path, dst, PATH_MAX);
    if (!destination_is_dir) return 1;
    i = str_length(dst_path);
    if (i > 0) {
        c = buf_get(dst_path, i - 1);
        if (c != '\\' && c != '/' && c != ':') {
            if (i >= PATH_MAX - 1) return 0;
            buf_set(dst_path, i, '\\');
            i = i + 1;
            buf_set(dst_path, i, 0);
        }
    }
    j = 0;
    while (buf_get(name, j) != 0) {
        if (i >= PATH_MAX - 1) return 0;
        buf_set(dst_path, i, buf_get(name, j));
        i = i + 1;
        j = j + 1;
    }
    buf_set(dst_path, i, 0);
    return 1;
}

static int copy_actual_source(char *path, char *name)
{
    int hin;
    int hout;
    int n;
    int i;
    int done;
    int own_output;
    hin = dos_open(path, 0);
    if (hin == -1) return 0;
    own_output = 0;
    hout = output_handle;
    if (destination_is_dir && source_has_wild && !source_has_plus) {
        if (!build_destination(name)) {
            dos_close(hin);
            return 0;
        }
        hout = dos_create(dst_path, 0);
        own_output = 1;
    } else if (hout == -1) {
        if (!build_destination(name)) {
            dos_close(hin);
            return 0;
        }
        hout = dos_create(dst_path, 0);
        output_handle = hout;
    }
    if (hout == -1) {
        dos_close(hin);
        return 0;
    }
    done = 0;
    while (!done) {
        n = dos_read(hin, buf, 512);
        if (n == -1) {
            if (own_output) dos_close(hout);
            dos_close(hin);
            return 0;
        }
        if (n == 0) break;
        if (opt_a) {
            i = 0;
            while (i < n) {
                if (buf_get(buf, i) == 26) {
                    n = i;
                    done = 1;
                    break;
                }
                i = i + 1;
            }
        }
        if (n > 0 && dos_write(hout, buf, n) != n) {
            if (own_output) dos_close(hout);
            dos_close(hin);
            return 0;
        }
    }
    if (own_output) dos_close(hout);
    dos_close(hin);
    copied_count = copied_count + 1;
    return 1;
}

static int copy_source_part(void)
{
    int attr;
    int found;
    int i;
    int c;
    if (!has_wildcard(src_part)) {
        if (!copy_basename(src_part, match_name)) return 0;
        str_copy_limit(src_path, src_part, PATH_MAX);
        return copy_actual_source(src_path, match_name);
    }
    dos_set_dta(dta);
    if (dos_find_first(src_part, 0x27) == -1) return 0;
    found = 0;
    while (1) {
        attr = buf_get(dta, 0x15);
        if (!(attr & 0x10)) {
            i = 0;
            while (i < 15) {
                c = buf_get(dta, 0x1E + i);
                buf_set(match_name, i, c);
                if (c == 0) break;
                i = i + 1;
            }
            buf_set(match_name, i, 0);
            if (!build_source_path(src_part, match_name)) return 0;
            if (!copy_actual_source(src_path, match_name)) return 0;
            found = 1;
        }
        if (dos_find_next() == -1) break;
    }
    return found;
}

static int copy_all_sources(void)
{
    int i;
    int o;
    int c;
    i = 0;
    while (1) {
        o = 0;
        while (1) {
            c = buf_get(src, i);
            if (c == 0 || c == '+') break;
            if (o < PATH_MAX - 1) {
                buf_set(src_part, o, c);
                o = o + 1;
            }
            i = i + 1;
        }
        buf_set(src_part, o, 0);
        if (o == 0) return 0;
        if (!copy_source_part()) return 0;
        if (c == 0) break;
        i = i + 1;
    }
    return 1;
}

int main(void)
{
    int c;
    int attr;
    int ok;
    opt_v = 0;
    opt_a = 0;
    opt_b = 1;
    buf_set(src, 0, 0);
    buf_set(dst, 0, 0);
    args_init();
    while (args_skip()) {
        c = peek_byte(arg_ptr);
        if (c == '/' || c == '-') {
            if (!eat_switch()) {
                print_dollar(msg_u);
                return 1;
            }
        } else if (buf_get(src, 0) == 0) {
            args_token(src, PATH_MAX);
        } else if (buf_get(dst, 0) == 0) {
            args_token(dst, PATH_MAX);
        } else {
            print_dollar(msg_u);
            return 1;
        }
    }
    if (buf_get(src, 0) == 0 || buf_get(dst, 0) == 0) {
        print_dollar(msg_u);
        return 1;
    }
    destination_is_dir = 0;
    c = str_length(dst);
    if (c > 0 && (buf_get(dst, c - 1) == '\\' || buf_get(dst, c - 1) == '/')) {
        destination_is_dir = 1;
    } else {
        attr = dos_chmod(dst, 0, 0);
        if (attr != -1 && (attr & 0x10)) destination_is_dir = 1;
    }
    source_has_plus = has_plus(src);
    source_has_wild = has_wildcard(src);
    output_handle = -1;
    copied_count = 0;
    if (opt_v) verify_begin();
    ok = copy_all_sources();
    if (output_handle != -1) dos_close(output_handle);
    if (opt_v) verify_end();
    if (!ok || copied_count == 0) {
        print_dollar(msg_err);
        return 1;
    }
    if (opt_v) print_dollar(msg_ok);
    else print_dollar(msg_copied);
    return 0;
}
