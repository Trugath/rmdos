"""Host-side checks for the in-tree wcc Small-C compiler."""

from __future__ import annotations

from scripts.wcc import Compiler


def _compile(src: str, *, com: bool = False) -> str:
    return Compiler(src, filename="<test>", com_entry=com).compile()


def test_for_increment_after_body() -> None:
    """for (i=0; i<3; i=i+1) must run body before increment."""
    asm = _compile(
        """
void main(void) {
    int i;
    int a;
    a = 0;
    for (i = 0; i < 3; i = i + 1) {
        a = a + 1;
    }
}
"""
    )
    lines = [ln.strip() for ln in asm.splitlines() if ln.strip()]

    cond_idx = None
    je_idx = None
    body_store_idx = None
    incr_store_idx = None
    jmp_cond_idx = None
    for i, ln in enumerate(lines):
        if ln == "_L0:" and cond_idx is None:
            cond_idx = i
            continue
        if cond_idx is not None and je_idx is None and ln.startswith("je "):
            je_idx = i
            continue
        if je_idx is not None and body_store_idx is None and ln.startswith("mov [bp"):
            body_store_idx = i
            continue
        if body_store_idx is not None and incr_store_idx is None and ln.startswith("mov [bp"):
            incr_store_idx = i
            continue
        if incr_store_idx is not None and ln.startswith("jmp _L0"):
            jmp_cond_idx = i
            break

    assert cond_idx is not None, asm
    assert je_idx is not None and je_idx > cond_idx, asm
    assert body_store_idx is not None and body_store_idx > je_idx, asm
    assert incr_store_idx is not None and incr_store_idx > body_store_idx, asm
    assert jmp_cond_idx is not None and jmp_cond_idx > incr_store_idx, asm


def test_for_empty_clauses() -> None:
    _compile(
        """
void main(void) {
    for (;;) {
        break;
    }
}
"""
    )
    _compile(
        """
void main(void) {
    int i;
    for (i = 0;;) {
        break;
    }
}
"""
    )
    _compile(
        """
void main(void) {
    int i;
    i = 0;
    for (; i < 3;) {
        i = i + 1;
        break;
    }
}
"""
    )


def test_for_with_call_in_increment() -> None:
    asm = _compile(
        """
int bump(int x) {
    return x + 1;
}
void main(void) {
    int i;
    for (i = 0; i < 2; i = bump(i)) {
        i = i;
    }
}
"""
    )
    assert "call bump" in asm


def test_array_arg_decays_to_pointer() -> None:
    asm = _compile(
        """
void take(char *p) { }
void main(void) {
    char buf[8];
    take(buf);
}
"""
    )
    # Must LEA the array address, not MOV the first element.
    assert "lea ax, [bp" in asm or "lea ax, [buf]" in asm
    # The push for take(buf) should not be mov ax, [buf] for a global;
    # for local, ensure we don't mov from the array slot as value before call.
    lines = [ln.strip() for ln in asm.splitlines()]
    call_i = next(i for i, ln in enumerate(lines) if ln == "call take")
    window = lines[call_i - 6 : call_i]
    assert any(ln.startswith("lea ax,") for ln in window), asm


def test_character_literals_compile_as_immediates() -> None:
    asm = _compile(
        """
int is_space(int c) {
    return c == ' ';
}
"""
    )
    assert "mov ax, 32" in asm
    assert '.asciz " "' not in asm


if __name__ == "__main__":
    test_for_increment_after_body()
    test_for_empty_clauses()
    test_for_with_call_in_increment()
    test_array_arg_decays_to_pointer()
    test_character_literals_compile_as_immediates()
    print("test_wcc: OK")
