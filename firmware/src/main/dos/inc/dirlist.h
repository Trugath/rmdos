#ifndef DIRLIST_H
#define DIRLIST_H

#define DIR_ENT_SIZE 24
#define DIR_MAX_ENTS 256

extern char dir_dta[128];
extern char dir_pool[6144];
extern char dir_keys[4];
extern char dir_revs[4];
extern int dir_nent;
extern int dir_nkeys;

int dir_ent_off(int idx);
int dir_u16_cmp(int a, int b);
int dir_name_cmp(int a, int b);
int dir_ext_at(int off);
int dir_ext_cmp(int a, int b);
int dir_key_cmp(int a, int b, int key);
int dir_ent_cmp(int a, int b);
void dir_ent_swap(int a, int b);
void dir_sort_pool(void);
void dir_store_dta(void);
void dir_load_dta(int idx);

#endif
