/* EDIT.COM — fullscreen editor with AH=48 16KiB buffer + find. */
#include "dos.h"

#define FALLBACK_MAX 4096
#define HEAP_MAX 16384
#define HEAP_PARAS 0x400
#define XFER_MAX 256
#define COLS 80
#define ROWS 24
#define STATUS_ROW 24
#define FIND_MAX 40

static char text_near[FALLBACK_MAX] = { 0 };
static char xfer[XFER_MAX] = { 0 };
static char path[64] = { 0 };
static char find_pat[FIND_MAX] = { 0 };
static char msg_ok[10] = "EDIT OK\r\n$";
static char msg_big[14] = "EDIT BIG OK\r\n$";
static char msg_u[28] = "EDIT [file] [/Q]\r\n$";
static char msg_err[22] = "EDIT: save failed\r\n$";
static char msg_no[20] = "EDIT: no file\r\n$";
static char msg_quit[28] = "Quit without save (Y/N)? $";
static char msg_find[12] = "Find: $";
static char msg_crlf[3] = "\r\n$";

static int text_seg;
static int buf_cap;
static int text_len;
static int cursor;
static int dirty;
static int quiet;
static int have_path;
static int view_row;
static int key_ax;
static int v_off;
static int v_ch;
static int v_at;
static int find_len;
static int t_tmp;

static int text_get(int i)
{
    if (text_seg != 0) {
        t_tmp = i;
        asm("push es");
        asm("mov es, [text_seg]");
        asm("mov bx, [t_tmp]");
        asm("mov al, es:[bx]");
        asm("mov ah, 0");
        asm("pop es");
    } else {
        return buf_get(text_near, i);
    }
}

static void text_set(int i, int val)
{
    if (text_seg != 0) {
        t_tmp = i;
        asm("push es");
        asm("mov es, [text_seg]");
        asm("mov bx, [t_tmp]");
        asm("mov ax, [bp+4]");
        asm("mov es:[bx], al");
        asm("pop es");
        return;
    }
    buf_set(text_near, i, val);
}

static void bios_key(void)
{
    asm("mov ah, 0");
    asm("int 0x16");
    asm("mov [key_ax], ax");
    reload_ds();
}

static void set_cursor(int row, int col)
{
    asm("mov dh, byte ptr [bp+6]");
    asm("mov dl, byte ptr [bp+4]");
    asm("mov bh, 0");
    asm("mov ah, 0x02");
    asm("int 0x10");
    reload_ds();
}

static void put_xy(int row, int col, int ch, int attr)
{
    v_off = (row * COLS + col) * 2;
    v_ch = ch;
    v_at = attr;
    asm("push es");
    asm("mov ax, 0xB800");
    asm("mov es, ax");
    asm("mov bx, [v_off]");
    asm("mov al, byte ptr [v_ch]");
    asm("mov ah, byte ptr [v_at]");
    asm("mov es:[bx], ax");
    asm("pop es");
    reload_ds();
}

static void cls_edit(void)
{
    int r;
    int c;
    r = 0;
    while (r < 25) {
        c = 0;
        while (c < COLS) {
            put_xy(r, c, ' ', 0x07);
            c = c + 1;
        }
        r = r + 1;
    }
}

static int line_start(int pos)
{
    while (pos > 0 && text_get(pos - 1) != 10) {
        pos = pos - 1;
    }
    return pos;
}

static int line_col(int pos)
{
    int s;
    s = line_start(pos);
    return pos - s;
}

static int next_line(int pos)
{
    while (pos < text_len) {
        if (text_get(pos) == 10) {
            return pos + 1;
        }
        pos = pos + 1;
    }
    return text_len;
}

static int nth_line_start(int n)
{
    int pos;
    int i;
    pos = 0;
    i = 0;
    while (i < n && pos < text_len) {
        pos = next_line(pos);
        i = i + 1;
    }
    return pos;
}

static int count_lines(void)
{
    int pos;
    int n;
    if (text_len == 0) {
        return 1;
    }
    pos = 0;
    n = 0;
    while (pos < text_len) {
        pos = next_line(pos);
        n = n + 1;
    }
    if (text_len > 0 && text_get(text_len - 1) == 10) {
        n = n + 1;
    }
    return n;
}

static int cursor_row(void)
{
    int row;
    int p;
    p = 0;
    row = 0;
    while (p < cursor && p < text_len) {
        if (text_get(p) == 10) {
            row = row + 1;
        }
        p = p + 1;
    }
    return row;
}

static void ensure_view(void)
{
    int row;
    int n;
    row = cursor_row();
    if (row < view_row) {
        view_row = row;
    }
    if (row >= view_row + ROWS) {
        view_row = row - ROWS + 1;
    }
    if (view_row < 0) {
        view_row = 0;
    }
    n = count_lines();
    if (view_row > 0 && view_row >= n) {
        view_row = n - 1;
    }
}

static void put_status_str(int col, char *s)
{
    int i;
    int ch;
    i = 0;
    while (col < COLS) {
        ch = buf_get(s, i);
        if (ch == 0 || ch == '$') {
            break;
        }
        put_xy(STATUS_ROW, col, ch, 0x70);
        i = i + 1;
        col = col + 1;
    }
}

static void put_status_num(int col, int n)
{
    char digits[6];
    int i;
    int d;
    if (n < 0) {
        n = 0;
    }
    if (n == 0) {
        put_xy(STATUS_ROW, col, '0', 0x70);
        return;
    }
    i = 0;
    while (n > 0 && i < 5) {
        d = n % 10;
        buf_set(digits, i, d + '0');
        n = n / 10;
        i = i + 1;
    }
    while (i > 0) {
        i = i - 1;
        put_xy(STATUS_ROW, col, buf_get(digits, i), 0x70);
        col = col + 1;
    }
}

static void draw_status(void)
{
    int c;
    int i;
    char ch;
    int row;
    int col;
    c = 0;
    while (c < COLS) {
        put_xy(STATUS_ROW, c, ' ', 0x70);
        c = c + 1;
    }
    put_status_str(0, "EDIT F2=Save F3=Find$");
    if (dirty) {
        put_xy(STATUS_ROW, 22, '*', 0x70);
    }
    row = cursor_row() + 1;
    col = line_col(cursor) + 1;
    put_xy(STATUS_ROW, 24, 'L', 0x70);
    put_status_num(25, row);
    put_xy(STATUS_ROW, 30, 'C', 0x70);
    put_status_num(31, col);
    put_xy(STATUS_ROW, 36, 'B', 0x70);
    put_status_num(37, buf_cap);
    i = 0;
    c = 48;
    while (c < COLS - 1 && have_path) {
        ch = buf_get(path, i);
        if (ch == 0) {
            break;
        }
        put_xy(STATUS_ROW, c, ch, 0x70);
        i = i + 1;
        c = c + 1;
    }
}

static void redraw(void)
{
    int r;
    int c;
    int pos;
    int ch;
    int row;
    int col;
    ensure_view();
    pos = nth_line_start(view_row);
    r = 0;
    while (r < ROWS) {
        c = 0;
        while (c < COLS) {
            ch = ' ';
            if (pos < text_len) {
                ch = text_get(pos);
                if (ch == 10) {
                    while (c < COLS) {
                        put_xy(r, c, ' ', 0x07);
                        c = c + 1;
                    }
                    pos = pos + 1;
                    break;
                }
                if (ch == 13) {
                    pos = pos + 1;
                    continue;
                }
                put_xy(r, c, ch, 0x07);
                pos = pos + 1;
                c = c + 1;
            } else {
                put_xy(r, c, ' ', 0x07);
                c = c + 1;
            }
        }
        if (c >= COLS && pos < text_len && text_get(pos) != 10) {
            while (pos < text_len && text_get(pos) != 10) {
                pos = pos + 1;
            }
            if (pos < text_len && text_get(pos) == 10) {
                pos = pos + 1;
            }
        }
        r = r + 1;
    }
    draw_status();
    row = cursor_row();
    col = line_col(cursor);
    if (col >= COLS) {
        col = COLS - 1;
    }
    set_cursor(row - view_row, col);
}

static int save_file(void)
{
    int h;
    int n;
    int off;
    int chunk;
    int w;
    if (!have_path) {
        print_dollar(msg_no);
        return 0;
    }
    h = dos_create(path, 0);
    if (h == -1) {
        return 0;
    }
    off = 0;
    while (off < text_len) {
        chunk = text_len - off;
        if (chunk > XFER_MAX) {
            chunk = XFER_MAX;
        }
        n = 0;
        while (n < chunk) {
            buf_set(xfer, n, text_get(off + n));
            n = n + 1;
        }
        w = dos_write(h, xfer, chunk);
        if (w != chunk) {
            dos_close(h);
            return 0;
        }
        off = off + chunk;
    }
    dos_close(h);
    dirty = 0;
    return 1;
}

static int load_file(void)
{
    int h;
    int n;
    int off;
    text_len = 0;
    cursor = 0;
    dirty = 0;
    if (!have_path) {
        return 1;
    }
    h = dos_open(path, 0);
    if (h == -1) {
        return 1;
    }
    off = 0;
    while (off < buf_cap - 1) {
        n = buf_cap - 1 - off;
        if (n > XFER_MAX) {
            n = XFER_MAX;
        }
        n = dos_read(h, xfer, n);
        if (n == -1) {
            n = 0;
        }
        if (n == 0) {
            break;
        }
        {
            int i;
            i = 0;
            while (i < n) {
                text_set(off + i, buf_get(xfer, i));
                i = i + 1;
            }
        }
        off = off + n;
    }
    dos_close(h);
    text_len = off;
    text_set(text_len, 0);
    return 1;
}

static void insert_char(int ch)
{
    int i;
    if (text_len >= buf_cap - 1) {
        return;
    }
    i = text_len;
    while (i > cursor) {
        text_set(i, text_get(i - 1));
        i = i - 1;
    }
    text_set(cursor, ch);
    text_len = text_len + 1;
    cursor = cursor + 1;
    dirty = 1;
}

static void delete_char(void)
{
    int i;
    if (cursor >= text_len) {
        return;
    }
    i = cursor;
    while (i < text_len - 1) {
        text_set(i, text_get(i + 1));
        i = i + 1;
    }
    text_len = text_len - 1;
    dirty = 1;
}

static void backspace(void)
{
    if (cursor < 1) {
        return;
    }
    cursor = cursor - 1;
    delete_char();
}

static void move_up(void)
{
    int col;
    int s;
    col = line_col(cursor);
    s = line_start(cursor);
    if (s == 0) {
        return;
    }
    s = line_start(s - 1);
    cursor = s;
    while (col > 0 && cursor < text_len && text_get(cursor) != 10) {
        cursor = cursor + 1;
        col = col - 1;
    }
}

static void move_down(void)
{
    int col;
    int s;
    col = line_col(cursor);
    s = next_line(cursor);
    if (s == cursor && s >= text_len) {
        return;
    }
    cursor = s;
    while (col > 0 && cursor < text_len && text_get(cursor) != 10) {
        cursor = cursor + 1;
        col = col - 1;
    }
}

static int ci_eq(int a, int b)
{
    return toupper_ch(a) == toupper_ch(b);
}

static int find_at(int start)
{
    int i;
    int j;
    int ch;
    if (find_len < 1) {
        return -1;
    }
    i = start;
    while (i + find_len <= text_len) {
        j = 0;
        while (j < find_len) {
            ch = text_get(i + j);
            if (!ci_eq(ch, buf_get(find_pat, j))) {
                break;
            }
            j = j + 1;
        }
        if (j == find_len) {
            return i;
        }
        i = i + 1;
    }
    return -1;
}

static void do_find(int prompt)
{
    int i;
    int ascii;
    int scan;
    int at;
    if (prompt) {
        set_cursor(STATUS_ROW, 0);
        {
            int c;
            c = 0;
            while (c < COLS) {
                put_xy(STATUS_ROW, c, ' ', 0x70);
                c = c + 1;
            }
        }
        put_status_str(0, "Find: $");
        set_cursor(STATUS_ROW, 6);
        find_len = 0;
        while (1) {
            bios_key();
            ascii = key_ax & 255;
            scan = (key_ax >> 8) & 255;
            if (ascii == 13) {
                break;
            }
            if (ascii == 27) {
                return;
            }
            if (ascii == 8) {
                if (find_len > 0) {
                    find_len = find_len - 1;
                    buf_set(find_pat, find_len, 0);
                    put_xy(STATUS_ROW, 6 + find_len, ' ', 0x70);
                    set_cursor(STATUS_ROW, 6 + find_len);
                }
                continue;
            }
            if (ascii >= 32 && ascii < 127 && find_len < FIND_MAX - 1) {
                buf_set(find_pat, find_len, ascii);
                put_xy(STATUS_ROW, 6 + find_len, ascii, 0x70);
                find_len = find_len + 1;
                buf_set(find_pat, find_len, 0);
                set_cursor(STATUS_ROW, 6 + find_len);
            }
        }
    }
    at = find_at(cursor + (prompt ? 0 : 1));
    if (at < 0) {
        at = find_at(0);
    }
    if (at >= 0) {
        cursor = at;
    }
    redraw();
}

static int copy_path(char *src)
{
    int i;
    i = 0;
    while (1) {
        buf_set(path, i, buf_get(src, i));
        if (buf_get(src, i) == 0) {
            break;
        }
        i = i + 1;
        if (i >= 63) {
            buf_set(path, i, 0);
            break;
        }
    }
    have_path = 1;
    return 1;
}

static void init_buffer(void)
{
    text_seg = dos_alloc(HEAP_PARAS);
    if (text_seg != 0) {
        buf_cap = HEAP_MAX;
    } else {
        text_seg = 0;
        buf_cap = FALLBACK_MAX;
    }
}

int main(void)
{
    char tok[64];
    int ascii;
    int scan;

    quiet = 0;
    have_path = 0;
    text_len = 0;
    cursor = 0;
    dirty = 0;
    view_row = 0;
    find_len = 0;
    init_buffer();

    args_init();
    while (args_skip()) {
        if (!args_token(tok, 64)) {
            break;
        }
        if (buf_get(tok, 0) == '/' || buf_get(tok, 0) == '-') {
            if (toupper_ch(buf_get(tok, 1)) == 'Q') {
                quiet = 1;
            }
        } else {
            copy_path(tok);
        }
    }

    if (quiet) {
        if (!have_path) {
            print_dollar(msg_u);
            return 1;
        }
        load_file();
        if (!save_file()) {
            print_dollar(msg_err);
            return 1;
        }
        if (text_len > FALLBACK_MAX) {
            print_dollar(msg_big);
        } else {
            print_dollar(msg_ok);
        }
        if (text_seg != 0) {
            dos_free(text_seg);
        }
        return 0;
    }

    load_file();
    cls_edit();
    redraw();

    while (1) {
        bios_key();
        ascii = key_ax & 255;
        scan = (key_ax >> 8) & 255;
        if (ascii == 27) {
            if (dirty) {
                set_cursor(STATUS_ROW, 0);
                print_dollar(msg_quit);
                bios_key();
                ascii = toupper_ch(key_ax & 255);
                if (ascii != 'Y') {
                    redraw();
                    continue;
                }
            }
            break;
        }
        if (ascii == 6) {
            do_find(1);
            continue;
        }
        if (scan == 0x3D) {
            /* F3 find-next */
            do_find(0);
            continue;
        }
        if (scan == 0x3C) {
            if (!save_file()) {
                set_cursor(STATUS_ROW, 40);
                print_dollar(msg_err);
            } else {
                redraw();
            }
            continue;
        }
        if (scan == 0x4B) {
            if (cursor > 0) {
                cursor = cursor - 1;
            }
            redraw();
            continue;
        }
        if (scan == 0x4D) {
            if (cursor < text_len) {
                cursor = cursor + 1;
            }
            redraw();
            continue;
        }
        if (scan == 0x48) {
            move_up();
            redraw();
            continue;
        }
        if (scan == 0x50) {
            move_down();
            redraw();
            continue;
        }
        if (scan == 0x53) {
            delete_char();
            redraw();
            continue;
        }
        if (ascii == 8) {
            backspace();
            redraw();
            continue;
        }
        if (ascii == 13) {
            insert_char(13);
            insert_char(10);
            redraw();
            continue;
        }
        if (ascii >= 32 && ascii < 127) {
            insert_char(ascii);
            redraw();
            continue;
        }
    }

    set_cursor(0, 0);
    print_dollar(msg_crlf);
    if (text_seg != 0) {
        dos_free(text_seg);
    }
    return 0;
}
