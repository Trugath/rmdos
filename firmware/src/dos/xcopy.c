/* XCOPY.COM — XCOPY src dst [/S][/E][/P][/V][/A][/D[:mm-dd-yy]]. */
#include "dos.h"

#define PATH_MAX 64
#define MAX_DEPTH 8

static char src[PATH_MAX];
static char dst[PATH_MAX];
static char file_pattern[PATH_MAX];
static char search_pattern[PATH_MAX];
static char source_path[PATH_MAX];
static char destination_path[PATH_MAX];
static char entry_name[16];
static char source_dirs[512];
static char destination_dirs[512];
static char dta_frames[1024];
static char buf[128];
static char msg_ok[10] = "copied\r\n$";
static char msg_e[16] = "XCOPY failed\r\n$";
static char msg_u[44] = "XCOPY src dst [/S][/E][/P][/V][/A][/D]\r\n$";
static char msg_prompt[8] = " (Y/N)? $";
static int opt_s;
static int opt_e;
static int opt_p;
static int opt_v;
static int opt_a;
static int opt_d;
static int opt_d_date;           /* nonzero = compare against fixed date */
static int d_date_val;           /* DOS date word */
static int verify_saved;
static int source_is_dir;
static int source_has_wild;
static int destination_is_dir;
static int copied_count;

static int xcopy_mkdir(char *path)
{
    asm("mov dx, [bp+4]");
    asm("mov ah, 0x39");
    asm("int 0x21");
    asm("mov ax, 0");
    asm("jnc Lxcopy_mkdir_ok");
    asm("mov ax, 0xFFFF");
    asm("Lxcopy_mkdir_ok:");
}

static int xcopy_rmdir(char *path)
{
    asm("mov dx, [bp+4]");
    asm("mov ah, 0x3A");
    asm("int 0x21");
    asm("mov ax, 0");
    asm("jnc Lxcopy_rmdir_ok");
    asm("mov ax, 0xFFFF");
    asm("Lxcopy_rmdir_ok:");
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

static int read_yn(void)
{
    int c;
    asm("mov ah, 0x01");
    asm("int 0x21");
    asm("mov ah, 0");
    asm("mov [dos_tmp], ax");
    reload_ds();
    c = dos_tmp;
    if (c >= 'a' && c <= 'z') c = c - 32;
    return c;
}

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

static int path_has_wildcard(char *path)
{
    int i;
    int c;
    i = 0;
    while (1) {
        c = buf_get(path, i);
        if (c == 0) return 0;
        if (c == '*' || c == '?') return 1;
        i = i + 1;
    }
}

static int path_ends_separator(char *path)
{
    int n;
    int c;
    n = str_length(path);
    if (n == 0) return 0;
    c = buf_get(path, n - 1);
    if (c == '\\' || c == '/') return 1;
    return 0;
}

static void trim_separator(char *path)
{
    int n;
    n = str_length(path);
    while (n > 1 && (buf_get(path, n - 1) == '\\' || buf_get(path, n - 1) == '/')) {
        if (n == 3 && buf_get(path, 1) == ':') break;
        n = n - 1;
        buf_set(path, n, 0);
    }
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

static int copy_entry_name(char *dta)
{
    int i;
    int c;
    i = 0;
    while (i < 15) {
        c = buf_get(dta, 0x1E + i);
        buf_set(entry_name, i, c);
        if (c == 0) return 1;
        i = i + 1;
    }
    buf_set(entry_name, 15, 0);
    return 1;
}

static int is_dot_directory(void)
{
    if (buf_get(entry_name, 0) != '.') return 0;
    if (buf_get(entry_name, 1) == 0) return 1;
    if (buf_get(entry_name, 1) == '.' && buf_get(entry_name, 2) == 0) return 1;
    return 0;
}

static int ensure_directory(char *path)
{
    int attr;
    attr = dos_chmod(path, 0, 0);
    if (attr != -1) {
        if (attr & 0x10) return 1;
        return 0;
    }
    if (xcopy_mkdir(path) == -1) return 0;
    return 1;
}

static int dta_date(char *dta)
{
    return buf_get(dta, 0x18) | (buf_get(dta, 0x19) << 8);
}

static int dta_time(char *dta)
{
    return buf_get(dta, 0x16) | (buf_get(dta, 0x17) << 8);
}

static int da_handle;
static int da_date;
static int da_time;

/* Return 1 if source should be copied under /D rules. */
static int date_allows(char *dta, char *dest_path)
{
    int sdate;
    int stime;
    int h;

    sdate = dta_date(dta);
    stime = dta_time(dta);
    /* Also allow date 0 (mkfs unset) through /D:date filters. */
    if (opt_d_date) {
        if (sdate != 0 && sdate < d_date_val) return 0;
        return 1;
    }
    /* bare /D: copy if dest missing or source newer (AH=57, no find). */
    h = dos_open(dest_path, 0);
    if (h == -1) return 1;
    da_handle = h;
    da_date = 0;
    da_time = 0;
    asm("mov bx, [da_handle]");
    asm("mov ax, 0x5700");
    asm("int 0x21");
    asm("jc Lda_done");
    asm("mov [da_date], dx");
    asm("mov [da_time], cx");
    asm("Lda_done:");
    reload_ds();
    dos_close(h);
    if (sdate > da_date) return 1;
    if (sdate < da_date) return 0;
    if (stime > da_time) return 1;
    return 0;
}

static int prompt_copy(char *path)
{
    int c;
    if (!opt_p) return 1;
    print_string(path);
    print_dollar(msg_prompt);
    c = read_yn();
    print_dollar("\r\n$");
    if (c == 'Y') return 1;
    return 0;
}

static int copy_file(char *from, char *to)
{
    int hin;
    int hout;
    int n;
    hin = dos_open(from, 0);
    if (hin == -1) return 0;
    hout = dos_create(to, 0);
    if (hout == -1) {
        dos_close(hin);
        return 0;
    }
    while (1) {
        n = dos_read(hin, buf, 128);
        if (n == -1) {
            dos_close(hout);
            dos_close(hin);
            return 0;
        }
        if (n == 0) break;
        if (dos_write(hout, buf, n) != n) {
            dos_close(hout);
            dos_close(hin);
            return 0;
        }
    }
    dos_close(hout);
    dos_close(hin);
    copied_count = copied_count + 1;
    return 1;
}

static int want_recurse(void)
{
    if (opt_s || opt_e) return 1;
    return 0;
}

static int copy_files_at_depth(int depth)
{
    int dta;
    int attr;
    int count;
    int src_dir;
    int dst_dir;
    dta = buf_addr(dta_frames, depth * 128);
    src_dir = buf_addr(source_dirs, depth * PATH_MAX);
    dst_dir = buf_addr(destination_dirs, depth * PATH_MAX);
    if (!join_path(search_pattern, src_dir, file_pattern)) return -1;
    dos_set_dta(dta);
    if (dos_find_first(search_pattern, 0x27) == -1) return 0;
    count = 0;
    while (1) {
        attr = buf_get(dta, 0x15);
        if (!(attr & 0x10)) {
            if (!opt_a || (attr & 0x20)) {
                copy_entry_name(dta);
                if (!join_path(source_path, src_dir, entry_name)) return -1;
                if (destination_is_dir) {
                    if (!join_path(destination_path, dst_dir, entry_name)) return -1;
                } else {
                    str_copy_limit(destination_path, dst, PATH_MAX);
                }
                if (!opt_d || date_allows(dta, destination_path)) {
                    if (prompt_copy(source_path)) {
                        if (!copy_file(source_path, destination_path)) return -1;
                        if (opt_a) {
                            attr = attr & ~0x20;
                            dos_chmod(source_path, 1, attr);
                        }
                        count = count + 1;
                    }
                }
            }
        }
        if (dos_find_next() == -1) break;
    }
    return count;
}

static int walk_tree(int depth)
{
    int dta;
    int src_dir;
    int dst_dir;
    int child_src;
    int child_dst;
    int attr;
    int n;
    int before;
    n = copy_files_at_depth(depth);
    if (n < 0) return 0;
    if (!want_recurse()) return 1;
    dta = buf_addr(dta_frames, depth * 128);
    src_dir = buf_addr(source_dirs, depth * PATH_MAX);
    dst_dir = buf_addr(destination_dirs, depth * PATH_MAX);
    if (!join_path(search_pattern, src_dir, "*.*")) return 0;
    dos_set_dta(dta);
    if (dos_find_first(search_pattern, 0x37) == -1) return 1;
    while (1) {
        attr = buf_get(dta, 0x15);
        if (attr & 0x10) {
            copy_entry_name(dta);
            if (!is_dot_directory()) {
                if (depth + 1 >= MAX_DEPTH) return 0;
                child_src = buf_addr(source_dirs, (depth + 1) * PATH_MAX);
                child_dst = buf_addr(destination_dirs, (depth + 1) * PATH_MAX);
                if (!join_path(child_src, src_dir, entry_name)) return 0;
                if (!join_path(child_dst, dst_dir, entry_name)) return 0;
                before = copied_count;
                if (!ensure_directory(child_dst)) return 0;
                if (!walk_tree(depth + 1)) return 0;
                /* /S without /E: drop empty destination directories */
                if (!opt_e && copied_count == before) {
                    xcopy_rmdir(child_dst);
                }
                dos_set_dta(dta);
            }
        }
        if (dos_find_next() == -1) break;
    }
    return 1;
}

static int parse_u8(void)
{
    int v;
    int c;
    int digits;
    v = 0;
    digits = 0;
    while (1) {
        c = peek_byte(arg_ptr);
        if (c < '0' || c > '9') break;
        v = v * 10 + (c - '0');
        digits = digits + 1;
        arg_ptr = arg_ptr + 1;
        if (digits > 2) return -1;
    }
    if (digits == 0) return -1;
    return v;
}

static int parse_d_date(void)
{
    int mo;
    int da;
    int yr;
    int c;
    /* expect :mm-dd-yy */
    c = peek_byte(arg_ptr);
    if (c != ':') return 0;
    arg_ptr = arg_ptr + 1;
    mo = parse_u8();
    if (mo < 1 || mo > 12) return 0;
    if (peek_byte(arg_ptr) != '-') return 0;
    arg_ptr = arg_ptr + 1;
    da = parse_u8();
    if (da < 1 || da > 31) return 0;
    if (peek_byte(arg_ptr) != '-') return 0;
    arg_ptr = arg_ptr + 1;
    yr = parse_u8();
    if (yr < 0) return 0;
    if (yr < 80) yr = yr + 100; /* 00-79 → 2000+ */
    yr = yr - 80;               /* years since 1980 */
    d_date_val = (yr << 9) | (mo << 5) | da;
    opt_d_date = 1;
    return 1;
}

static int parse_switch(void)
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
            arg_ptr = arg_ptr + 1;
            if (c == 'S') {
                opt_s = 1;
            } else if (c == 'E') {
                opt_e = 1;
            } else if (c == 'P') {
                opt_p = 1;
            } else if (c == 'V') {
                opt_v = 1;
            } else if (c == 'A') {
                opt_a = 1;
            } else if (c == 'D') {
                opt_d = 1;
                if (peek_byte(arg_ptr) == ':') {
                    if (!parse_d_date()) return 0;
                }
            } else {
                return 0;
            }
            seen = 1;
        }
    }
    return seen;
}

static int prepare_source(void)
{
    int attr;
    int i;
    int last;
    int c;
    int o;
    int root;
    root = buf_addr(source_dirs, 0);
    str_copy_limit(root, src, PATH_MAX);
    source_is_dir = 0;
    if (path_ends_separator(root)) {
        source_is_dir = 1;
        trim_separator(root);
    } else {
        attr = dos_chmod(root, 0, 0);
        if (attr != -1 && (attr & 0x10)) source_is_dir = 1;
    }
    if (source_is_dir) {
        str_copy_limit(file_pattern, "*.*", PATH_MAX);
        source_has_wild = 1;
        return 1;
    }
    i = 0;
    last = -1;
    while (1) {
        c = buf_get(src, i);
        if (c == 0) break;
        if (c == '\\' || c == '/' || c == ':') last = i;
        i = i + 1;
    }
    o = 0;
    i = last + 1;
    while (buf_get(src, i) != 0) {
        buf_set(file_pattern, o, buf_get(src, i));
        o = o + 1;
        i = i + 1;
    }
    buf_set(file_pattern, o, 0);
    if (o == 0) return 0;
    o = 0;
    while (o <= last) {
        buf_set(root, o, buf_get(src, o));
        o = o + 1;
    }
    buf_set(root, o, 0);
    source_has_wild = path_has_wildcard(file_pattern);
    return 1;
}

int main(void)
{
    int c;
    int attr;
    int root_dst;
    int ok;
    opt_s = 0;
    opt_e = 0;
    opt_p = 0;
    opt_v = 0;
    opt_a = 0;
    opt_d = 0;
    opt_d_date = 0;
    d_date_val = 0;
    copied_count = 0;
    buf_set(src, 0, 0);
    buf_set(dst, 0, 0);
    args_init();
    while (args_skip()) {
        c = peek_byte(arg_ptr);
        if (c == '/' || c == '-') {
            if (!parse_switch()) {
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
    if (buf_get(src, 0) == 0 || buf_get(dst, 0) == 0 || !prepare_source()) {
        print_dollar(msg_u);
        return 1;
    }
    root_dst = buf_addr(destination_dirs, 0);
    str_copy_limit(root_dst, dst, PATH_MAX);
    destination_is_dir = 0;
    if (path_ends_separator(root_dst)) {
        destination_is_dir = 1;
        trim_separator(root_dst);
    } else {
        attr = dos_chmod(root_dst, 0, 0);
        if (attr != -1 && (attr & 0x10)) destination_is_dir = 1;
    }
    if (source_is_dir || source_has_wild || want_recurse()) destination_is_dir = 1;
    if (destination_is_dir && !ensure_directory(root_dst)) {
        print_dollar(msg_e);
        return 1;
    }
    if (opt_v) verify_begin();
    ok = walk_tree(0);
    if (opt_v) verify_end();
    if (!ok || copied_count == 0) {
        print_dollar(msg_e);
        return 1;
    }
    print_dollar(msg_ok);
    return 0;
}
