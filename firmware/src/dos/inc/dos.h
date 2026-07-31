/* Minimal DOS INT 21h helpers for wcc-built .COM programs. */

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
