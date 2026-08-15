/*
 * Starfield demo for rmDOS (8088 / CGA).
 * CGA mode 4 (320x200, 4 colors). Stars move toward center (tunnel effect).
 * Bresenham in pixel space so each step = 1 pixel.
 * Direct VRAM updates (no BIOS AH=0Ch).
 */

#include "dos.h"

#define CGA_ROW_BYTES   80
/* Mode 4/5: odd scanlines start 8KB into B800, not at byte 8000. Using 8000
 * shears odd vs even rows (stepped borders, vertical "seam"). */
#define CGA_BANK_SIZE   0x2000
#define CGA_FB_SIZE     0x4000
#define CENTER_X       158
#define CENTER_Y       100
#define NSTARS         120
#define COLORS         4
#define CGA_WHITE      3
#define SCREEN_W       320
#define SCREEN_H       200
#define VSYNC_WAIT     1
#define TICKS_PER_FRAME 0
#define N_BORDER_ANGLES 128

#define SCALE 128
#define COS128 128
#define SIN128 6

static int star_x[NSTARS] = { 0 };
static int star_y[NSTARS] = { 0 };
static int star_color[NSTARS] = { 0 };
static int star_err[NSTARS] = { 0 };
static int key_ready_val = 0;
static int rnd_state = 42;
static int last_tick = 0;
static int bios_tick_val = 0;
static int bresenham_next_x = 0;
static int bresenham_next_y = 0;
static int bresenham_next_err = 0;
static int respawn_x = 0;
static int respawn_y = 0;

static int pix_off = 0;
static int pix_shift = 0;
static int pix_color = 0;

static int border_x[N_BORDER_ANGLES] = { 0 };
static int border_y[N_BORDER_ANGLES] = { 0 };

static void init_border_circle(void)
{
    int i, dx, dy, t, t_min, x, y;
    dx = SCALE;
    dy = 0;
    i = 0;
    while (i < N_BORDER_ANGLES) {
        t_min = 32767;
        x = CENTER_X;
        y = CENTER_Y;
        if (dx > 0) {
            t = (319 - CENTER_X) * SCALE / dx;
            if (t > 0 && t < t_min) {
                t_min = t;
                x = 319;
                y = CENTER_Y + (dy * t) / SCALE;
            }
        }
        if (dx < 0) {
            t = (0 - CENTER_X) * SCALE / dx;
            if (t > 0 && t < t_min) {
                t_min = t;
                x = 0;
                y = CENTER_Y + (dy * t) / SCALE;
            }
        }
        if (dy > 0) {
            t = (199 - CENTER_Y) * SCALE / dy;
            if (t > 0 && t < t_min) {
                t_min = t;
                x = CENTER_X + (dx * t) / SCALE;
                y = 199;
            }
        }
        if (dy < 0) {
            t = (0 - CENTER_Y) * SCALE / dy;
            if (t > 0 && t < t_min) {
                t_min = t;
                x = CENTER_X + (dx * t) / SCALE;
                y = 0;
            }
        }
        if (t_min == 32767) {
            x = CENTER_X;
            y = CENTER_Y;
        }
        if (x < 0) x = 0;
        if (x >= SCREEN_W) x = SCREEN_W - 1;
        if (y < 0) y = 0;
        if (y >= SCREEN_H) y = SCREEN_H - 1;
        border_x[i] = x;
        border_y[i] = y;
        t = (dx * COS128 - dy * SIN128) / SCALE;
        dy = (dx * SIN128 + dy * COS128) / SCALE;
        dx = t;
        i = i + 1;
    }
}

static int bresenham_step_pixel(int x0, int y0, int x1, int y1, int err_val)
{
    int dx, sx, dy, sy, err, e2;

    if (x0 == x1 && y0 == y1) {
        bresenham_next_x = x0;
        bresenham_next_y = y0;
        return 1;
    }
    dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
    sx = (x0 < x1) ? 1 : -1;
    dy = (y0 < y1) ? -(y1 - y0) : -(y0 - y1);
    sy = (y0 < y1) ? 1 : -1;
    err = err_val;
    e2 = 2 * err;
    if (e2 >= dy) {
        err = err + dy;
        x0 = x0 + sx;
    }
    if (e2 <= dx) {
        err = err + dx;
        y0 = y0 + sy;
    }
    if (x0 < 0) x0 = 0;
    if (x0 >= SCREEN_W) x0 = SCREEN_W - 1;
    if (y0 < 0) y0 = 0;
    if (y0 >= SCREEN_H) y0 = SCREEN_H - 1;
    bresenham_next_x = x0;
    bresenham_next_y = y0;
    bresenham_next_err = err;
    if (x0 == x1 && y0 == y1)
        return 1;
    return 0;
}

static int rnd_next(int s)
{
    int lo;
    lo = s * 25173 + 13849;
    return lo & 0x7fff;
}

/* Walk `steps` Bresenham steps from (x,y) toward center; updates x,y,err via outs. */
static int walk_err;
static int walk_x;
static int walk_y;

static void walk_toward_center(int steps)
{
    int n;
    int done;

    n = 0;
    while (n < steps) {
        done = bresenham_step_pixel(walk_x, walk_y, CENTER_X, CENTER_Y, walk_err);
        walk_x = bresenham_next_x;
        walk_y = bresenham_next_y;
        walk_err = bresenham_next_err;
        if (done)
            n = steps;
        else
            n = n + 1;
    }
}

static void err_from_pos(int x, int y)
{
    int dx;
    int ady;

    dx = (CENTER_X >= x) ? (CENTER_X - x) : (x - CENTER_X);
    ady = (y < CENTER_Y) ? (CENTER_Y - y) : (y - CENTER_Y);
    walk_err = dx + (-ady);
}

/* Respawn just inside the border so erase does not chew the frame. */
static void respawn_on_edge(void)
{
    int i;

    rnd_state = rnd_next(rnd_state);
    i = rnd_state % N_BORDER_ANGLES;
    walk_x = border_x[i];
    walk_y = border_y[i];
    err_from_pos(walk_x, walk_y);
    walk_toward_center(3);
    respawn_x = walk_x;
    respawn_y = walk_y;
}

/*
 * Place each star on a random border ray at a random depth.
 * Avoids LCG (x,y) modulo lattices that show up as lines/clusters.
 */
static void init_stars(void)
{
    int i;
    int s;
    int bi;
    int dist;
    int max_in;
    int steps;
    int ady;

    s = 42;
    rnd_state = 42;
    i = 0;
    while (i < NSTARS) {
        s = rnd_next(s);
        bi = s % N_BORDER_ANGLES;
        walk_x = border_x[bi];
        walk_y = border_y[bi];
        err_from_pos(walk_x, walk_y);
        dist = (CENTER_X >= walk_x) ? (CENTER_X - walk_x) : (walk_x - CENTER_X);
        ady = (walk_y < CENTER_Y) ? (CENTER_Y - walk_y) : (walk_y - CENTER_Y);
        if (ady > dist)
            dist = ady;
        max_in = dist - 10;
        if (max_in < 3)
            max_in = 3;
        s = rnd_next(s);
        steps = 3 + (s % max_in);
        walk_toward_center(steps);
        star_x[i] = walk_x;
        star_y[i] = walk_y;
        star_err[i] = walk_err;
        s = rnd_next(s);
        /* Colors 1..3 only (4 would become 0 after &3 and vanish). */
        star_color[i] = (s % 3) + 1;
        i = i + 1;
    }
}

static int on_frame_border(int x, int y)
{
    if (x <= 0)
        return 1;
    if (x >= SCREEN_W - 1)
        return 1;
    if (y <= 0)
        return 1;
    if (y >= SCREEN_H - 1)
        return 1;
    return 0;
}

void cga_set_mode_4(void);
void cga_clear_screen(void);
void cga_put_pixel(int x, int y, int color);
int  kbd_ready(void);
int  key_get(void);
void cga_vsync(void);
int  get_bios_tick(void);

void cga_set_mode_4(void)
{
    asm("push ds");
    asm("mov ax, 0x0004");
    asm("int 0x10");
    /* Keep BIOS palette (0x30): intense green/red/brown on black. */
    asm("pop ds");
}

void cga_clear_screen(void)
{
    asm("push es");
    asm("mov ax, 0xB800");
    asm("mov es, ax");
    asm("xor di, di");
    asm("xor ax, ax");
    asm("mov cx, 0x2000");
    asm("cld");
    asm("rep stosw");
    asm("pop es");
}

/*
 * Fast mode-4 putpixel. wcc pushes args left-to-right so:
 *   [bp+8]=x, [bp+6]=y, [bp+4]=color.
 */
void cga_put_pixel(int x, int y, int color)
{
    asm("push es");
    asm("push bx");
    asm("push cx");
    asm("push dx");
    asm("push si");
    asm("mov bx, [bp+8]");       /* x */
    asm("mov dx, [bp+6]");       /* y */
    asm("cmp bx, 0");
    asm("jl Lpp_done");
    asm("cmp bx, 320");
    asm("jge Lpp_done");
    asm("cmp dx, 0");
    asm("jl Lpp_done");
    asm("cmp dx, 200");
    asm("jge Lpp_done");
    asm("mov ax, dx");
    asm("shr ax, 1");            /* row_half */
    asm("mov cl, 4");
    asm("shl ax, cl");           /* *16 */
    asm("mov cx, ax");
    asm("shl ax, 1");            /* *32 */
    asm("shl ax, 1");            /* *64 */
    asm("add ax, cx");           /* *80 */
    asm("mov cx, bx");
    asm("shr cx, 1");
    asm("shr cx, 1");            /* col = x>>2 */
    asm("add ax, cx");           /* offset in even bank */
    asm("test dx, 1");
    asm("jz Lpp_even");
    asm("add ax, 0x2000");
    asm("Lpp_even:");
    asm("mov si, ax");           /* SI = offset */
    asm("mov cx, bx");
    asm("and cx, 3");
    asm("mov ax, 3");
    asm("sub ax, cx");
    asm("shl ax, 1");            /* shift = (3-(x&3))<<1 */
    asm("mov cx, ax");           /* CL = shift */
    asm("mov ax, 0xB800");
    asm("mov es, ax");
    asm("mov al, es:[si]");
    asm("mov ah, 3");
    asm("shl ah, cl");
    asm("not ah");
    asm("and al, ah");
    asm("mov ah, [bp+4]");
    asm("and ah, 3");
    asm("shl ah, cl");
    asm("or al, ah");
    asm("mov es:[si], al");
    asm("Lpp_done:");
    asm("pop si");
    asm("pop dx");
    asm("pop cx");
    asm("pop bx");
    asm("pop es");
}

static void draw_white_border(void)
{
    int x;
    int y;
    x = 0;
    while (x < SCREEN_W) {
        cga_put_pixel(x, 0, CGA_WHITE);
        cga_put_pixel(x, SCREEN_H - 1, CGA_WHITE);
        x = x + 1;
    }
    y = 0;
    while (y < SCREEN_H) {
        cga_put_pixel(0, y, CGA_WHITE);
        cga_put_pixel(SCREEN_W - 1, y, CGA_WHITE);
        y = y + 1;
    }
}

int kbd_ready(void)
{
    asm("push ds");
    asm("mov ah, 0x01");
    asm("int 0x16");
    asm("jz L_key_not_ready");
    asm("mov ax, 1");
    asm("jmp L_key_done");
    asm("L_key_not_ready:");
    asm("xor ax, ax");
    asm("L_key_done:");
    asm("pop ds");
    asm("push cs");
    asm("pop ds");
    asm("mov word ptr key_ready_val, ax");
    return key_ready_val;
}

int key_get(void)
{
    asm("push ds");
    asm("xor ah, ah");
    asm("int 0x16");
    asm("pop ds");
    asm("push cs");
    asm("pop ds");
    asm("mov word ptr key_ready_val, ax");
    return key_ready_val;
}

static void flush_keyboard(void)
{
    while (kbd_ready())
        key_get();
}

/* Wait for one vertical retrace; CX timeout so we never wedge forever. */
void cga_vsync(void)
{
    asm("push cx");
    asm("mov dx, 0x3DA");
    asm("mov cx, 0xFFFF");
    asm("L_vsync_off:");
    asm("in al, dx");
    asm("test al, 8");
    asm("jz L_vsync_on_init");
    asm("dec cx");
    asm("jnz L_vsync_off");
    asm("jmp L_vsync_done");
    asm("L_vsync_on_init:");
    asm("mov cx, 0xFFFF");
    asm("L_vsync_on:");
    asm("in al, dx");
    asm("test al, 8");
    asm("jnz L_vsync_done");
    asm("dec cx");
    asm("jnz L_vsync_on");
    asm("L_vsync_done:");
    asm("pop cx");
}

int get_bios_tick(void)
{
    asm("push ds");
    asm("push es");
    asm("mov ax, 0x40");
    asm("mov es, ax");
    asm("mov ax, [es:0x6C]");
    asm("pop es");
    asm("pop ds");
    asm("push cs");
    asm("pop ds");
    asm("mov word ptr bios_tick_val, ax");
    return bios_tick_val;
}

int main(void)
{
    int i;
    int d;
    int respawned;
    int dx;
    int ady_init;
    int old_x;
    int old_y;

    reload_ds();

    print_string("STAR:start\r\n");
    reload_ds();
    cga_set_mode_4();
    reload_ds();
    cga_clear_screen();
    init_border_circle();
    init_stars();
    draw_white_border();
    i = 0;
    while (i < NSTARS) {
        cga_put_pixel(star_x[i], star_y[i], star_color[i]);
        i = i + 1;
    }
    flush_keyboard();
    last_tick = get_bios_tick();
    reload_ds();

    while (1) {
        reload_ds();
        d = 0;
        while (d < VSYNC_WAIT) {
            cga_vsync();
            d = d + 1;
        }

        i = 0;
        while (i < NSTARS) {
            old_x = star_x[i];
            old_y = star_y[i];
            respawned = bresenham_step_pixel(star_x[i], star_y[i], CENTER_X, CENTER_Y, star_err[i]);
            if (respawned) {
                respawn_on_edge();
                star_x[i] = respawn_x;
                star_y[i] = respawn_y;
                dx = (CENTER_X >= respawn_x) ? (CENTER_X - respawn_x) : (respawn_x - CENTER_X);
                ady_init = (respawn_y < CENTER_Y) ? (CENTER_Y - respawn_y) : (respawn_y - CENTER_Y);
                star_err[i] = dx + (-ady_init);
            } else {
                star_x[i] = bresenham_next_x;
                star_y[i] = bresenham_next_y;
                star_err[i] = bresenham_next_err;
            }
            if (old_x != star_x[i] || old_y != star_y[i]) {
                /* Do not punch holes in the yellow frame. */
                if (on_frame_border(old_x, old_y))
                    cga_put_pixel(old_x, old_y, CGA_WHITE);
                else
                    cga_put_pixel(old_x, old_y, 0);
                cga_put_pixel(star_x[i], star_y[i], star_color[i]);
            }
            i = i + 1;
        }

        if (kbd_ready()) {
            key_get();
            break;
        }
    }

    asm("push ds");
    asm("mov ax, 0x0003");
    asm("int 0x10");
    asm("pop ds");

    reload_ds();
    print_string("STAR:done\r\n");
    print_string("Starfield done.\r\n");
    return 0;
}
