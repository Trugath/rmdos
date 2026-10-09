/* MEM.COM — walk conventional MCB chain; print free/total; optional EMS. */
#include "dos.h"

static int first_mcb;
static int mcb_seg;
static int mcb_owner;
static int mcb_paras;
static int mcb_id;
static int free_paras;
static int used_paras;
static int total_paras;
static int ems_ah;
static int ems_free;
static int ems_total;
static char msg_hdr[28] = "Conventional Memory:\r\n$";
static char msg_ems[24] = "Expanded Memory:\r\n$";
static char msg_tot[14] = "Total: $";
static char msg_used[14] = "Used:  $";
static char msg_free[14] = "Free:  $";
static char msg_kb[8] = " KB\r\n$";

static void load_first_mcb(void)
{
    asm("mov ah, 0x52");
    asm("int 0x21");
    asm("push cs");
    asm("pop ds");
    asm("mov ax, es:[bx]");
    asm("mov [first_mcb], ax");
}

static void load_mcb(int seg)
{
    asm("mov ax, [bp+4]");
    asm("mov es, ax");
    asm("mov al, es:[0]");
    asm("mov ah, 0");
    asm("mov [mcb_id], ax");
    asm("mov ax, es:[1]");
    asm("mov [mcb_owner], ax");
    asm("mov ax, es:[3]");
    asm("mov [mcb_paras], ax");
    asm("push cs");
    asm("pop ds");
}

static void print_kb_from_paras(int paras)
{
    /* paras * 16 / 1024 = paras / 64 */
    print_num(paras / 64);
    print_dollar(msg_kb);
}

static void print_kb_from_pages(int pages)
{
    /* 16 KB pages */
    print_num(pages * 16);
    print_dollar(msg_kb);
}

static int ems_present(void)
{
    asm("mov ax, 0x3567");
    asm("int 0x21");
    asm("cmp word ptr es:[0x0A], 0x4D45");
    asm("jne Lems_no");
    asm("cmp word ptr es:[0x0C], 0x584D");
    asm("jne Lems_no");
    asm("cmp word ptr es:[0x0E], 0x5858");
    asm("jne Lems_no");
    asm("cmp word ptr es:[0x10], 0x3058");
    asm("jne Lems_no");
    asm("mov ah, 0x40");
    asm("int 0x67");
    asm("mov [ems_ah], ah");
    asm("push cs");
    asm("pop ds");
    asm("jmp Lems_done");
    asm("Lems_no:");
    asm("push cs");
    asm("pop ds");
    asm("mov byte ptr [ems_ah], 0xFF");
    asm("Lems_done:");
    return ems_ah == 0;
}

static void load_ems_counts(void)
{
    asm("mov ah, 0x42");
    asm("int 0x67");
    asm("mov [ems_ah], ah");
    asm("mov [ems_free], bx");
    asm("mov [ems_total], dx");
    asm("push cs");
    asm("pop ds");
}

int main(void)
{
    free_paras = 0;
    used_paras = 0;
    load_first_mcb();
    mcb_seg = first_mcb;
    while (1) {
        load_mcb(mcb_seg);
        if (mcb_owner == 0) {
            free_paras = free_paras + mcb_paras;
        } else {
            used_paras = used_paras + mcb_paras;
        }
        /* Include the MCB paragraph itself in totals. */
        total_paras = free_paras + used_paras;
        if (mcb_id == 'Z' || mcb_id == 'z') {
            break;
        }
        if (mcb_id != 'M' && mcb_id != 'm') {
            break;
        }
        mcb_seg = mcb_seg + mcb_paras + 1;
    }
    print_dollar(msg_hdr);
    print_dollar(msg_tot);
    print_kb_from_paras(total_paras);
    print_dollar(msg_used);
    print_kb_from_paras(used_paras);
    print_dollar(msg_free);
    print_kb_from_paras(free_paras);

    if (ems_present()) {
        load_ems_counts();
        if (ems_ah == 0) {
            print_dollar(msg_ems);
            print_dollar(msg_tot);
            print_kb_from_pages(ems_total);
            print_dollar(msg_free);
            print_kb_from_pages(ems_free);
        }
    }
    return 0;
}
