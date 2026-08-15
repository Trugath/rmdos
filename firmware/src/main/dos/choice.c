/* CHOICE.COM — prompt for a key among choices. */
#include "dos.h"

static char def_choices[3] = "YN";
static char choices[32];
static char msg_prompt_end[5] = "]? $";
static int no_prompt;
static int have_timeout;
static int def_choice;
static int timeout_sec;
static int start_tick;
static int tick_limit;

static void copy_def(void)
{
    int i;
    i = 0;
    while (1) {
        buf_set(choices, i, buf_get(def_choices, i));
        if (buf_get(def_choices, i) == 0) {
            break;
        }
        i = i + 1;
    }
}

static int find_choice(int key)
{
    int i;
    int c;
    i = 0;
    while (1) {
        c = buf_get(choices, i);
        if (c == 0) {
            return -1;
        }
        if (c == key) {
            return i + 1;
        }
        i = i + 1;
    }
}

static void eat_colon(void)
{
    if (peek_byte(arg_ptr) == ':') {
        arg_ptr = arg_ptr + 1;
    }
}

int main(void)
{
    int c;
    int i;
    int first;
    int idx;
    int n;

    copy_def();
    no_prompt = 0;
    have_timeout = 0;
    def_choice = 'Y';
    timeout_sec = 0;
    args_init();
    while (args_skip()) {
        c = peek_byte(arg_ptr);
        if (c != '/' && c != '-') {
            break;
        }
        arg_ptr = arg_ptr + 1;
        c = toupper_ch(peek_byte(arg_ptr));
        arg_ptr = arg_ptr + 1;
        if (c == 'N') {
            no_prompt = 1;
        } else if (c == 'C') {
            eat_colon();
            i = 0;
            while (1) {
                c = peek_byte(arg_ptr);
                if (c == ' ' || c == 9 || c == 13 || c == 0) {
                    break;
                }
                if (c == '/' || c == '-') {
                    break;
                }
                buf_set(choices, i, toupper_ch(c));
                i = i + 1;
                arg_ptr = arg_ptr + 1;
            }
            buf_set(choices, i, 0);
            if (i == 0) {
                copy_def();
            }
        } else if (c == 'T') {
            eat_colon();
            c = peek_byte(arg_ptr);
            if (c != 13 && c != 0) {
                def_choice = toupper_ch(c);
                arg_ptr = arg_ptr + 1;
                if (peek_byte(arg_ptr) == ',') {
                    arg_ptr = arg_ptr + 1;
                    n = 0;
                    while (1) {
                        c = peek_byte(arg_ptr);
                        if (c < '0' || c > '9') {
                            break;
                        }
                        n = n * 10 + (c - '0');
                        arg_ptr = arg_ptr + 1;
                    }
                    timeout_sec = n;
                    have_timeout = 1;
                }
            }
        }
    }
    if (!no_prompt) {
        print_char('[');
        first = 1;
        i = 0;
        while (1) {
            c = buf_get(choices, i);
            if (c == 0) {
                break;
            }
            if (!first) {
                print_char(',');
            }
            first = 0;
            print_char(c);
            i = i + 1;
        }
        print_dollar(msg_prompt_end);
    }
    if (have_timeout) {
        start_tick = bios_tick_lo();
        tick_limit = timeout_sec * 18;
    }
    while (1) {
        if (key_ready()) {
            c = toupper_ch(read_key());
            idx = find_choice(c);
            if (idx >= 0) {
                return idx;
            }
        } else if (have_timeout) {
            if ((bios_tick_lo() - start_tick) >= tick_limit) {
                idx = find_choice(def_choice);
                if (idx >= 0) {
                    return idx;
                }
                return 1;
            }
        }
    }
}
