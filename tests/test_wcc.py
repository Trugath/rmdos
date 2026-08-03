"""Host-side checks for the in-tree wcc Small-C compiler."""

from __future__ import annotations

import re

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


def test_continue_in_while_and_for() -> None:
    asm_while = _compile(
        """
void main(void) {
    int i;
    i = 0;
    while (i < 3) {
        i = i + 1;
        continue;
        i = 99;
    }
}
"""
    )
    assert "continue outside loop" not in asm_while
    assert "jmp _L0" in asm_while  # while continue -> loop head

    asm_for = _compile(
        """
void main(void) {
    int i;
    int a;
    a = 0;
    for (i = 0; i < 3; i = i + 1) {
        if (i == 1) {
            continue;
        }
        a = a + 1;
    }
}
"""
    )
    # for continue jumps to incr label, not condition
    assert re.search(r"jmp _L\d+", asm_for)


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


def test_object_like_macro() -> None:
    asm = _compile(
        """
#define N 42
int main(void) {
    return N;
}
"""
    )
    assert "mov ax, 42" in asm


def test_function_like_macro_snap4() -> None:
    asm = _compile(
        """
#define SNAP4(x) ((x) & ~3)
int main(void) {
    int a;
    a = SNAP4(7);
    return a;
}
"""
    )
    assert "mov ax, 7" in asm
    assert "and" in asm


def test_function_like_macro_two_args() -> None:
    asm = _compile(
        """
#define ADD(a, b) ((a) + (b))
int main(void) {
    return ADD(10, 20);
}
"""
    )
    assert "mov ax, 10" in asm
    assert "mov ax, 20" in asm or "add ax," in asm


def test_function_like_requires_paren_to_expand() -> None:
    from scripts.wcc import CompileError

    try:
        _compile(
            """
#define FOO(x) (x)
int main(void) {
    return FOO;
}
"""
        )
        raise AssertionError("expected undefined identifier for FOO without call")
    except CompileError as e:
        assert "FOO" in e.msg


def test_define_space_before_paren_is_object_like() -> None:
    asm = _compile(
        """
#define FOO (3)
int main(void) {
    return FOO;
}
"""
    )
    assert "mov ax, 3" in asm


def test_nested_macro_expansion() -> None:
    asm = _compile(
        """
#define INNER(x) ((x) + 1)
#define OUTER(y) INNER(y)
int main(void) {
    return OUTER(5);
}
"""
    )
    # Expansion yields INNER(5) → ((5) + 1); both immediates must appear.
    assert "mov ax, 5" in asm
    assert "mov ax, 1" in asm


def test_extern_scalar_and_incomplete_array() -> None:
    asm = _compile(
        """
extern int flag;
extern char blit_data[];
int main(void) {
    return flag;
}
"""
    )
    assert ".extern flag" in asm
    assert ".extern blit_data" in asm
    assert "mov ax, [flag]" in asm
    # No storage for externs
    assert not re.search(r"^flag:", asm, re.M)
    assert not re.search(r"^blit_data:", asm, re.M)
    # Parsing continues past extern (main is emitted)
    assert "main:" in asm


def test_extern_array_decays_to_lea() -> None:
    asm = _compile(
        """
extern char buf[];
void take(char *p) { }
void main(void) {
    take(buf);
}
"""
    )
    assert ".extern buf" in asm
    assert "lea ax, [buf]" in asm


def test_extern_with_initializer_rejected() -> None:
    from scripts.wcc import CompileError

    try:
        _compile("extern int x = 1;\nvoid main(void) { }\n")
        raise AssertionError("expected CompileError")
    except CompileError as e:
        assert "initializer" in e.msg


def test_extern_function_prototype() -> None:
    asm = _compile(
        """
extern int helper(int x);
int main(void) {
    return helper(3);
}
"""
    )
    assert "call helper" in asm
    assert "helper:" not in asm.split("main:")[0]


if __name__ == "__main__":
    test_for_increment_after_body()
    test_for_empty_clauses()
    test_for_with_call_in_increment()
    test_array_arg_decays_to_pointer()
    test_continue_in_while_and_for()
    test_character_literals_compile_as_immediates()
    test_object_like_macro()
    test_function_like_macro_snap4()
    test_function_like_macro_two_args()
    test_function_like_requires_paren_to_expand()
    test_define_space_before_paren_is_object_like()
    test_nested_macro_expansion()
    test_extern_scalar_and_incomplete_array()
    test_extern_array_decays_to_lea()
    test_extern_with_initializer_rejected()
    test_extern_function_prototype()
    print("test_wcc: OK")
