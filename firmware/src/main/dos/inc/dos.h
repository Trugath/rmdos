/* Minimal DOS INT 21h helpers for rmcc-built .COM programs.
 *
 * Calling convention: rmcc pushes args left-to-right, so for f(a,b,c):
 *   [bp+8]=a, [bp+6]=b, [bp+4]=c.
 * After BIOS calls that may clobber DS, call reload_ds().
 * Implementations live in dos.c (linked into each C .COM).
 */

#ifndef DOS_H
#define DOS_H

/* FindFirst/Next attribute masks (AH=4Eh/4Fh). */
#define FA_NORMAL 0
#define FA_LABEL  8
#define FA_DIRENT 0x10
#define FA_FILES  0x27   /* R+H+S+A — non-directory files */
#define FA_ALL    0x37   /* FA_FILES | FA_DIRENT */


struct DiskFree {
    int free_clusters;
    int secs_per_clust;
    int bytes_per_sect;
    int total_clusters;
};

extern int dos_tmp;
extern int overlay_ds;
extern char *arg_ptr;

void reload_ds(void);
void dos_exit(int code);
void print_char(int c);
void print_string(char *s);
void print_dollar(char *s);
void print_num(int n);
void print_u32(int lo, int hi);
int read_key(void);
int key_ready(void);
int peek_byte(char *addr);
void poke_byte(char *addr, int val);
int peek_word(char *addr);
void poke_word(char *addr, int val);
int buf_get(char *buf, int i);
void buf_set(char *buf, int i, int val);
char *buf_addr(char *buf, int i);
void args_init(void);
int args_skip(void);
int args_token(char *buf, int maxlen);
int args_token_quoted(char *buf, int maxlen);
int toupper_ch(int c);
int dos_open(char *path, int mode);
int dos_create(char *path, int attr);
void dos_close(int handle);
int dos_read(int handle, char *buf, int len);
int dos_write(int handle, char *buf, int len);
int dos_delete(char *path);
int dos_rename(char *src, char *dst);
int dos_mkdir(char *path);
int dos_rmdir(char *path);
void dos_set_dta(char *dta);
int dos_find_first(char *pattern, int attr);
int dos_find_next(void);
int dos_chmod(char *path, int mode, int attr);
int dos_disk_free(int drive, struct DiskFree *out);
int bios_tick_lo(void);
int get_cwd(char *buf);
int screen_page_lines(void);
int dos_alloc(int paras);
int dos_free(int seg);
int dos_get_psp(void);
int far_peek(int seg, int off);
void far_poke(int seg, int off, int val);
int env_seg(void);
void env_set_seg(int seg);
int dos_exec_overlay(char *path, int load_seg, int reloc);
int dos_exec(char *path);
int dos_far_call(int seg, int off);

#endif
