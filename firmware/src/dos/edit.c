/* EDIT.COM — classic-lite fullscreen text editor (4096-byte buffer). */
#include "dos.h"

#define BUF_MAX 4096
#define COLS 80
#define ROWS 24
#define STATUS_ROW 24

static char text[BUF_MAX] = { 0 };
static char path[64] = { 0 };
static char msg_ok[10] = "EDIT OK\r\n$";
static char msg_u[28] = "EDIT [file] [/Q]\r\n$";
static char msg_err[22] = "EDIT: save failed\r\n$";
static char msg_no[20] = "EDIT: no file\r\n$";
static char msg_quit[28] = "Quit without save (Y/N)? $";
static char msg_crlf[3] = "\r\n$";

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
    while (pos > 0 && buf_get(text, pos - 1) != 10) {
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
        if (buf_get(text, pos) == 10) {
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
    if (text_len > 0 && buf_get(text, text_len - 1) == 10) {
        n = n + 1;
    }
    return n;
}

static void ensure_view(void)
{
    int row;
    int pos;
    int n;
    pos = 0;
    row = 0;
    while (pos < cursor && pos < text_len) {
        if (buf_get(text, pos) == 10) {
            row = row + 1;
        }
        pos = pos + 1;
    }
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

static void draw_status(void)
{
    int c;
    int i;
    char ch;
    c = 0;
    while (c < COLS) {
        put_xy(STATUS_ROW, c, ' ', 0x70);
        c = c + 1;
    }
    put_xy(STATUS_ROW, 0, 'E', 0x70);
    put_xy(STATUS_ROW, 1, 'D', 0x70);
    put_xy(STATUS_ROW, 2, 'I', 0x70);
    put_xy(STATUS_ROW, 3, 'T', 0x70);
    put_xy(STATUS_ROW, 5, 'F', 0x70);
    put_xy(STATUS_ROW, 6, '2', 0x70);
    put_xy(STATUS_ROW, 7, '=', 0x70);
    put_xy(STATUS_ROW, 8, 'S', 0x70);
    put_xy(STATUS_ROW, 9, 'a', 0x70);
    put_xy(STATUS_ROW, 10, 'v', 0x70);
    put_xy(STATUS_ROW, 11, 'e', 0x70);
    if (dirty) {
        put_xy(STATUS_ROW, 13, '*', 0x70);
    }
    i = 0;
    c = 16;
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
    ensure_view();
    pos = nth_line_start(view_row);
    r = 0;
    while (r < ROWS) {
        c = 0;
        while (c < COLS) {
            ch = ' ';
            if (pos < text_len) {
                ch = buf_get(text, pos);
                if (ch == 10) {
                    ch = ' ';
                    /* fill rest of row */
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
        if (c >= COLS && pos < text_len && buf_get(text, pos) != 10) {
            /* soft-wrap remainder of long line: skip to LF */
            while (pos < text_len && buf_get(text, pos) != 10) {
                pos = pos + 1;
            }
            if (pos < text_len && buf_get(text, pos) == 10) {
                pos = pos + 1;
            }
        }
        r = r + 1;
    }
    draw_status();
    {
        int row;
        int col;
        int p;
        p = 0;
        row = 0;
        while (p < cursor && p < text_len) {
            if (buf_get(text, p) == 10) {
                row = row + 1;
            }
            p = p + 1;
        }
        col = line_col(cursor);
        if (col >= COLS) {
            col = COLS - 1;
        }
        set_cursor(row - view_row, col);
    }
}

static int save_file(void)
{
    int h;
    int n;
    if (!have_path) {
        print_dollar(msg_no);
        return 0;
    }
    h = dos_create(path, 0);
    if (h == -1) {
        return 0;
    }
    if (text_len > 0) {
        n = dos_write(h, text, text_len);
    } else {
        n = 0;
    }
    dos_close(h);
    if (n != text_len) {
        return 0;
    }
    dirty = 0;
    return 1;
}

static int load_file(void)
{
    int h;
    int n;
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
    n = dos_read(h, text, BUF_MAX - 1);
    dos_close(h);
    if (n < 0) {
        n = 0;
    }
    text_len = n;
    buf_set(text, text_len, 0);
    return 1;
}

static void insert_char(int ch)
{
    int i;
    if (text_len >= BUF_MAX - 1) {
        return;
    }
    i = text_len;
    while (i > cursor) {
        buf_set(text, i, buf_get(text, i - 1));
        i = i - 1;
    }
    buf_set(text, cursor, ch);
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
        buf_set(text, i, buf_get(text, i + 1));
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
    while (col > 0 && cursor < text_len && buf_get(text, cursor) != 10) {
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
    while (col > 0 && cursor < text_len && buf_get(text, cursor) != 10) {
        cursor = cursor + 1;
        col = col - 1;
    }
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
        print_dollar(msg_ok);
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
        if (scan == 0x3C) {
            /* F2 */
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

    /* leave a clean line for COMMAND */
    set_cursor(0, 0);
    print_dollar(msg_crlf);
    return 0;
}
