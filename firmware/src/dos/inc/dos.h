/* Minimal DOS INT 21h helpers for wcc-built .COM programs.
 *
 * Calling convention: wcc pushes args left-to-right, so for f(a,b,c):
 *   [bp+8]=a, [bp+6]=b, [bp+4]=c.
 * After BIOS calls that may clobber DS, call reload_ds().
 */

static int dos_tmp;

void reload_ds(void)
{
    asm("push cs");
    asm("pop ds");
}

void dos_exit(int code)
{
    asm("mov ax, [bp+4]");
    asm("mov ah, 0x4C");
    asm("int 0x21");
}

void print_char(int c)
{
    asm("mov dx, [bp+4]");
    asm("mov ah, 0x02");
    asm("int 0x21");
}

void print_string(char *s)
{
    asm("mov si, [bp+4]");
    asm("Lps:");
    asm("lodsb");
    asm("test al, al");
    asm("jz Lps_done");
    asm("mov dl, al");
    asm("mov ah, 0x02");
    asm("int 0x21");
    asm("jmp Lps");
    asm("Lps_done:");
}

/* Print a $-terminated string (INT 21h AH=09). */
void print_dollar(char *s)
{
    asm("mov dx, [bp+4]");
    asm("mov ah, 0x09");
    asm("int 0x21");
}

/* Print unsigned 16-bit decimal (0..65535). */
void print_num(int n)
{
    char digits[6];
    int i;
    int d;
    if (n == 0) {
        print_char('0');
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
        print_char(buf_get(digits, i));
    }
}

static char print_numbuf[12];
static int print_u32_lo;
static int print_u32_hi;

/* Print unsigned 32-bit decimal from low/high 16-bit halves. */
void print_u32(int lo, int hi)
{
    print_u32_lo = lo;
    print_u32_hi = hi;
    asm("lea di, [print_numbuf+10]");
    asm("mov byte ptr [di], 0");
    asm("mov ax, [print_u32_lo]");
    asm("mov dx, [print_u32_hi]");
    asm("Lpu_loop:");
    asm("mov bx, 10");
    asm("mov si, ax");
    asm("mov cx, dx");
    asm("xor dx, dx");
    asm("mov ax, cx");
    asm("div bx");
    asm("mov cx, ax");
    asm("mov ax, si");
    asm("div bx");
    asm("mov si, ax");
    asm("add dl, '0'");
    asm("dec di");
    asm("mov [di], dl");
    asm("mov ax, si");
    asm("mov dx, cx");
    asm("mov bx, ax");
    asm("or bx, dx");
    asm("jnz Lpu_loop");
    asm("Lpu_out:");
    asm("mov al, [di]");
    asm("test al, al");
    asm("jz Lpu_done");
    asm("mov dl, al");
    asm("mov ah, 0x02");
    asm("int 0x21");
    asm("inc di");
    asm("jmp Lpu_out");
    asm("Lpu_done:");
}

int read_key(void)
{
    asm("mov ah, 0x08");
    asm("int 0x21");
    asm("mov ah, 0");
}

int key_ready(void)
{
    asm("mov ah, 0x0B");
    asm("int 0x21");
    asm("mov ah, 0");
}

int peek_byte(int addr)
{
    asm("mov bx, [bp+4]");
    asm("mov al, [bx]");
    asm("mov ah, 0");
}

void poke_byte(int addr, int val)
{
    asm("mov bx, [bp+6]");
    asm("mov ax, [bp+4]");
    asm("mov [bx], al");
}

int peek_word(int addr)
{
    asm("mov bx, [bp+4]");
    asm("mov ax, [bx]");
}

void poke_word(int addr, int val)
{
    asm("mov bx, [bp+6]");
    asm("mov ax, [bp+4]");
    asm("mov [bx], ax");
}

int buf_get(char *buf, int i)
{
    asm("mov bx, [bp+6]");
    asm("add bx, [bp+4]");
    asm("mov al, [bx]");
    asm("mov ah, 0");
}

void buf_set(char *buf, int i, int val)
{
    asm("mov bx, [bp+8]");
    asm("add bx, [bp+6]");
    asm("mov ax, [bp+4]");
    asm("mov [bx], al");
}

int buf_addr(char *buf, int i)
{
    asm("mov ax, [bp+6]");
    asm("add ax, [bp+4]");
}

static int arg_ptr;

void args_init(void)
{
    arg_ptr = 0x81;
}

/* Skip spaces/tabs. Returns 0 if at end (CR/NUL), else 1. */
int args_skip(void)
{
    int c;
    while (1) {
        c = peek_byte(arg_ptr);
        if (c != ' ' && c != 9) {
            break;
        }
        arg_ptr = arg_ptr + 1;
    }
    if (c == 13 || c == 0) {
        return 0;
    }
    return 1;
}

/* Copy next whitespace-delimited token into buf (NUL-terminated).
 * Returns 1 on success, 0 if no token. */
int args_token(char *buf, int maxlen)
{
    int i;
    int c;
    if (!args_skip()) {
        buf_set(buf, 0, 0);
        return 0;
    }
    i = 0;
    while (1) {
        c = peek_byte(arg_ptr);
        if (c == ' ' || c == 9 || c == 13 || c == 0) {
            break;
        }
        if (i < maxlen - 1) {
            buf_set(buf, i, c);
            i = i + 1;
        }
        arg_ptr = arg_ptr + 1;
    }
    buf_set(buf, i, 0);
    return 1;
}

/* If current char is '"', copy until closing quote; else args_token. */
int args_token_quoted(char *buf, int maxlen)
{
    int i;
    int c;
    if (!args_skip()) {
        buf_set(buf, 0, 0);
        return 0;
    }
    c = peek_byte(arg_ptr);
    if (c == 34) {
        arg_ptr = arg_ptr + 1;
        i = 0;
        while (1) {
            c = peek_byte(arg_ptr);
            if (c == 34 || c == 13 || c == 0) {
                break;
            }
            if (i < maxlen - 1) {
                buf_set(buf, i, c);
                i = i + 1;
            }
            arg_ptr = arg_ptr + 1;
        }
        if (c == 34) {
            arg_ptr = arg_ptr + 1;
        }
        buf_set(buf, i, 0);
        if (i > 0) {
            return 1;
        }
        return 0;
    }
    return args_token(buf, maxlen);
}

int toupper_ch(int c)
{
    if (c >= 'a' && c <= 'z') {
        return c - 32;
    }
    return c;
}

int dos_open(char *path, int mode)
{
    asm("mov dx, [bp+6]");
    asm("mov ax, [bp+4]");
    asm("mov ah, 0x3D");
    asm("int 0x21");
    asm("jnc Lop_ok");
    asm("mov ax, 0xFFFF");
    asm("Lop_ok:");
}

int dos_create(char *path, int attr)
{
    asm("mov dx, [bp+6]");
    asm("mov cx, [bp+4]");
    asm("mov ah, 0x3C");
    asm("int 0x21");
    asm("jnc Lcr_ok");
    asm("mov ax, 0xFFFF");
    asm("Lcr_ok:");
}

void dos_close(int handle)
{
    asm("mov bx, [bp+4]");
    asm("mov ah, 0x3E");
    asm("int 0x21");
}

/* Returns bytes read, 0 on EOF, -1 on error. */
int dos_read(int handle, char *buf, int len)
{
    asm("mov bx, [bp+8]");
    asm("mov dx, [bp+6]");
    asm("mov cx, [bp+4]");
    asm("mov ah, 0x3F");
    asm("int 0x21");
    asm("jnc Lrd_ok");
    asm("mov ax, 0xFFFF");
    asm("Lrd_ok:");
}

/* Returns bytes written, or -1 on error. */
int dos_write(int handle, char *buf, int len)
{
    asm("mov bx, [bp+8]");
    asm("mov dx, [bp+6]");
    asm("mov cx, [bp+4]");
    asm("mov ah, 0x40");
    asm("int 0x21");
    asm("jnc Lwr_ok");
    asm("mov ax, 0xFFFF");
    asm("Lwr_ok:");
}

int dos_delete(char *path)
{
    asm("mov dx, [bp+4]");
    asm("mov ah, 0x41");
    asm("int 0x21");
    asm("mov ax, 0");
    asm("jnc Ldl_ok");
    asm("mov ax, 0xFFFF");
    asm("Ldl_ok:");
}

/* Rename/move. Returns 0 on success, -1 on failure. */
int dos_rename(char *src, char *dst)
{
    asm("mov dx, [bp+6]");
    asm("mov di, [bp+4]");
    asm("mov ah, 0x56");
    asm("int 0x21");
    asm("mov ax, 0");
    asm("jnc Lrn_ok");
    asm("mov ax, 0xFFFF");
    asm("Lrn_ok:");
}

void dos_set_dta(char *dta)
{
    asm("mov dx, [bp+4]");
    asm("mov ah, 0x1A");
    asm("int 0x21");
}

/* Find first. Returns 0 on success, -1 on failure. */
int dos_find_first(char *pattern, int attr)
{
    asm("mov dx, [bp+6]");
    asm("mov cx, [bp+4]");
    asm("mov ah, 0x4E");
    asm("int 0x21");
    asm("mov ax, 0");
    asm("jnc Lff_ok");
    asm("mov ax, 0xFFFF");
    asm("Lff_ok:");
}

int dos_find_next(void)
{
    asm("mov ah, 0x4F");
    asm("int 0x21");
    asm("mov ax, 0");
    asm("jnc Lfn_ok");
    asm("mov ax, 0xFFFF");
    asm("Lfn_ok:");
}

/* mode 0=get (AX=attr), mode 1=set. Returns attr or -1. */
int dos_chmod(char *path, int mode, int attr)
{
    asm("mov dx, [bp+8]");
    asm("mov ax, [bp+6]");
    asm("mov cx, [bp+4]");
    asm("mov ah, 0x43");
    asm("int 0x21");
    asm("jnc Lch_ok");
    asm("mov ax, 0xFFFF");
    asm("jmp Lch_done");
    asm("Lch_ok:");
    asm("mov ax, cx");
    asm("Lch_done:");
}

static int df_ax;
static int df_bx;
static int df_cx;
static int df_dx;

int dos_disk_free(int drive)
{
    asm("mov dx, [bp+4]");
    asm("mov ah, 0x36");
    asm("int 0x21");
    asm("mov [df_ax], ax");
    asm("mov [df_bx], bx");
    asm("mov [df_cx], cx");
    asm("mov [df_dx], dx");
    asm("cmp ax, 0xFFFF");
    asm("jne Ldf_ok");
    asm("mov ax, 0xFFFF");
    asm("jmp Ldf_done");
    asm("Ldf_ok:");
    asm("mov ax, 0");
    asm("Ldf_done:");
}

int dos_df_free_clusters(void) { return df_ax; }
int dos_df_secs_per_clust(void) { return df_bx; }
int dos_df_bytes_per_sect(void) { return df_cx; }
int dos_df_total_clusters(void) { return df_dx; }

int bios_tick_lo(void)
{
    asm("xor ah, ah");
    asm("int 0x1A");
    asm("mov ax, dx");
}

int get_cwd(char *buf)
{
    asm("mov si, [bp+4]");
    asm("xor dl, dl");
    asm("mov ah, 0x47");
    asm("int 0x21");
    asm("mov ax, 0");
    asm("jnc Lcw_ok");
    asm("mov ax, 0xFFFF");
    asm("Lcw_ok:");
}

/* BDA rows-1 at 0040:0084; return page line count (default 24). */
int screen_page_lines(void)
{
    asm("push es");
    asm("mov ax, 0x0040");
    asm("mov es, ax");
    asm("mov al, es:[0x84]");
    asm("mov ah, 0");
    asm("pop es");
    asm("mov [dos_tmp], ax");
    if (dos_tmp < 1) {
        return 24;
    }
    return dos_tmp;
}
