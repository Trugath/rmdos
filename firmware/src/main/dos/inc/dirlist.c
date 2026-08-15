#include "dos.h"
#include "dirlist.h"

char dir_dta[128];
char dir_pool[1920];
char dir_keys[4];
char dir_revs[4];
int dir_nent;
int dir_nkeys;

int dir_ent_off(int idx)
{
    return idx * DIR_ENT_SIZE;
}

int dir_u16_cmp(int a, int b)
{
    if (a == b) {
        return 0;
    }
    if ((a & 0x8000) == (b & 0x8000)) {
        if (a < b) {
            return -1;
        }
        return 1;
    }
    if (a & 0x8000) {
        return 1;
    }
    return -1;
}

int dir_name_cmp(int a, int b)
{
    int i;
    int ca;
    int cb;
    int oa;
    int ob;

    oa = dir_ent_off(a);
    ob = dir_ent_off(b);
    i = 0;
    while (i < 13) {
        ca = buf_get(dir_pool, oa + i);
        cb = buf_get(dir_pool, ob + i);
        if (ca != cb) {
            if (ca < cb) {
                return -1;
            }
            return 1;
        }
        if (ca == 0) {
            return 0;
        }
        i = i + 1;
    }
    return 0;
}

int dir_ext_at(int off)
{
    int i;
    int c;

    i = 0;
    while (i < 13) {
        c = buf_get(dir_pool, off + i);
        if (c == 0) {
            return off + i;
        }
        if (c == '.') {
            return off + i + 1;
        }
        i = i + 1;
    }
    return off + i;
}

int dir_ext_cmp(int a, int b)
{
    int i;
    int ca;
    int cb;
    int oa;
    int ob;

    oa = dir_ext_at(dir_ent_off(a));
    ob = dir_ext_at(dir_ent_off(b));
    i = 0;
    while (i < 13) {
        ca = buf_get(dir_pool, oa + i);
        cb = buf_get(dir_pool, ob + i);
        if (ca != cb) {
            if (ca < cb) {
                return -1;
            }
            return 1;
        }
        if (ca == 0) {
            return 0;
        }
        i = i + 1;
    }
    return 0;
}

int dir_key_cmp(int a, int b, int key)
{
    int oa;
    int ob;
    int da;
    int db;
    int r;

    oa = dir_ent_off(a);
    ob = dir_ent_off(b);
    if (key == 'N') {
        return dir_name_cmp(a, b);
    }
    if (key == 'E') {
        r = dir_ext_cmp(a, b);
        if (r != 0) {
            return r;
        }
        return dir_name_cmp(a, b);
    }
    if (key == 'D') {
        da = peek_word(buf_addr(dir_pool, oa + 16));
        db = peek_word(buf_addr(dir_pool, ob + 16));
        r = dir_u16_cmp(da, db);
        if (r != 0) {
            return r;
        }
        da = peek_word(buf_addr(dir_pool, oa + 14));
        db = peek_word(buf_addr(dir_pool, ob + 14));
        return dir_u16_cmp(da, db);
    }
    if (key == 'S') {
        da = peek_word(buf_addr(dir_pool, oa + 20));
        db = peek_word(buf_addr(dir_pool, ob + 20));
        r = dir_u16_cmp(da, db);
        if (r != 0) {
            return r;
        }
        da = peek_word(buf_addr(dir_pool, oa + 18));
        db = peek_word(buf_addr(dir_pool, ob + 18));
        return dir_u16_cmp(da, db);
    }
    if (key == 'G') {
        da = buf_get(dir_pool, oa + 13) & FA_DIRENT;
        db = buf_get(dir_pool, ob + 13) & FA_DIRENT;
        if (da != 0 && db == 0) {
            return -1;
        }
        if (da == 0 && db != 0) {
            return 1;
        }
        return 0;
    }
    return 0;
}

int dir_ent_cmp(int a, int b)
{
    int i;
    int r;
    int key;

    i = 0;
    while (i < dir_nkeys) {
        key = buf_get(dir_keys, i);
        r = dir_key_cmp(a, b, key);
        if (r != 0) {
            if (buf_get(dir_revs, i)) {
                return 0 - r;
            }
            return r;
        }
        i = i + 1;
    }
    return dir_name_cmp(a, b);
}

void dir_ent_swap(int a, int b)
{
    int i;
    int t;
    int oa;
    int ob;

    oa = dir_ent_off(a);
    ob = dir_ent_off(b);
    i = 0;
    while (i < DIR_ENT_SIZE) {
        t = buf_get(dir_pool, oa + i);
        buf_set(dir_pool, oa + i, buf_get(dir_pool, ob + i));
        buf_set(dir_pool, ob + i, t);
        i = i + 1;
    }
}

void dir_sort_pool(void)
{
    int i;
    int j;

    i = 0;
    while (i < dir_nent) {
        j = i + 1;
        while (j < dir_nent) {
            if (dir_ent_cmp(i, j) > 0) {
                dir_ent_swap(i, j);
            }
            j = j + 1;
        }
        i = i + 1;
    }
}

void dir_store_dta(void)
{
    int off;
    int i;
    int c;

    if (dir_nent >= DIR_MAX_ENTS) {
        return;
    }
    off = dir_ent_off(dir_nent);
    i = 0;
    while (i < 13) {
        c = buf_get(dir_dta, 0x1E + i);
        buf_set(dir_pool, off + i, c);
        if (c == 0) {
            break;
        }
        i = i + 1;
    }
    while (i < 13) {
        buf_set(dir_pool, off + i, 0);
        i = i + 1;
    }
    buf_set(dir_pool, off + 13, buf_get(dir_dta, 0x15));
    poke_word(buf_addr(dir_pool, off + 14), peek_word(buf_addr(dir_dta, 0x16)));
    poke_word(buf_addr(dir_pool, off + 16), peek_word(buf_addr(dir_dta, 0x18)));
    poke_word(buf_addr(dir_pool, off + 18), peek_word(buf_addr(dir_dta, 0x1A)));
    poke_word(buf_addr(dir_pool, off + 20), peek_word(buf_addr(dir_dta, 0x1C)));
    dir_nent = dir_nent + 1;
}

void dir_load_dta(int idx)
{
    int off;
    int i;
    int c;

    off = dir_ent_off(idx);
    i = 0;
    while (i < 13) {
        c = buf_get(dir_pool, off + i);
        buf_set(dir_dta, 0x1E + i, c);
        i = i + 1;
    }
    buf_set(dir_dta, 0x15, buf_get(dir_pool, off + 13));
    poke_word(buf_addr(dir_dta, 0x16), peek_word(buf_addr(dir_pool, off + 14)));
    poke_word(buf_addr(dir_dta, 0x18), peek_word(buf_addr(dir_pool, off + 16)));
    poke_word(buf_addr(dir_dta, 0x1A), peek_word(buf_addr(dir_pool, off + 18)));
    poke_word(buf_addr(dir_dta, 0x1C), peek_word(buf_addr(dir_pool, off + 20)));
}
