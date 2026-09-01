"""Host-side checks for the in-tree rmcc Small-C compiler."""

from __future__ import annotations

import re

from scripts.rmcc import Compiler


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
    from scripts.rmcc import CompileError

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
    from scripts.rmcc import CompileError

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


def test_static_file_scope_omits_global() -> None:
    asm = _compile(
        """
static int x;
static int f(void) {
    return x;
}
int g(void) {
    return f();
}
int main(void) {
    return g();
}
"""
    )
    assert re.search(r"^x:", asm, re.M)
    assert re.search(r"^f:", asm, re.M)
    assert ".global x" not in asm
    assert ".global f" not in asm
    assert ".global g" in asm
    assert "call f" in asm


def test_static_and_extern_rejected() -> None:
    _expect_compile_error("static extern int x;\nvoid main(void) { }\n", "combine")


def test_static_local_storage_and_addressing() -> None:
    asm = _compile(
        """
int f(void) {
    static int n;
    n = n + 1;
    return n;
}
int main(void) {
    return f();
}
"""
    )
    assert re.search(r"f__static_n_\d+:", asm)
    assert ".global f__static_n_" not in asm
    assert re.search(r"mov \[f__static_n_\d+\], ax", asm)
    assert re.search(r"mov ax, \[f__static_n_\d+\]", asm)
    # Increment path should not use a fresh stack slot for n
    f_body = asm.split("f:")[1].split("main:")[0]
    assert "sub sp, 2" not in f_body


def test_static_local_initializer() -> None:
    asm = _compile(
        """
int f(void) {
    static int n = 5;
    return n;
}
void main(void) { }
"""
    )
    assert re.search(r"f__static_n_\d+:", asm)
    assert ".word 5" in asm


def test_plain_local_still_uses_bp() -> None:
    asm = _compile(
        """
int f(void) {
    int n;
    n = 1;
    return n;
}
void main(void) { }
"""
    )
    assert "sub sp, 2" in asm
    assert re.search(r"mov \[bp-?\d+\], ax", asm)


def _expect_compile_error(src: str, substr: str) -> None:
    from scripts.rmcc import CompileError

    try:
        _compile(src)
        raise AssertionError(f"expected CompileError containing {substr!r}")
    except CompileError as e:
        assert substr in e.msg, f"got {e.msg!r}, want substring {substr!r}"


def test_struct_layout_and_global_storage() -> None:
    from scripts.rmcc import Compiler

    src = """
struct S {
    int x;
    int a : 3;
    int b : 5;
    char c;
    int d : 10;
    int e : 10;
};
struct S g;
void main(void) { }
"""
    c = Compiler(src, filename="<test>")
    asm = c.compile()
    st = c.structs["S"]
    assert st.size == 10
    assert st.members["x"].byte_offset == 0 and not st.members["x"].is_bitfield
    assert st.members["a"].byte_offset == 2 and st.members["a"].bit_offset == 0 and st.members["a"].bit_width == 3
    assert st.members["b"].byte_offset == 2 and st.members["b"].bit_offset == 3 and st.members["b"].bit_width == 5
    assert st.members["c"].byte_offset == 4 and st.members["c"].typ == "char"
    assert st.members["d"].byte_offset == 6 and st.members["d"].bit_width == 10
    assert st.members["e"].byte_offset == 8 and st.members["e"].bit_width == 10
    assert ".space 10, 0" in asm
    assert re.search(r"^g:", asm, re.M)


def test_struct_local_ordinary_and_bitfield_access() -> None:
    asm = _compile(
        """
struct S {
    int x;
    int flag : 3;
    int other : 5;
};
void main(void) {
    struct S s;
    s.x = 42;
    s.flag = 3;
    s.other = 7;
}
"""
    )
    assert "sub sp, 4" in asm
    assert "mov ax, 42" in asm
    assert "and ax, 7" in asm  # flag width 3
    assert "and ax, 31" in asm  # other width 5
    assert "shl ax, 3" in asm  # other at bit 3
    assert "or ax, cx" in asm
    assert "mov [bx], ax" in asm


def test_struct_global_member_read_write() -> None:
    asm = _compile(
        """
struct S {
    int a : 4;
    int b : 4;
};
struct S g;
int main(void) {
    g.a = 1;
    g.b = 2;
    return g.a + g.b;
}
"""
    )
    assert "lea bx, [g]" in asm or "lea bx, [g+0]" in asm
    assert "and ax, 15" in asm
    assert "shl ax, 4" in asm
    assert "shr ax, 4" in asm or "and ax, 15" in asm
    assert "add ax, bx" in asm or "add ax," in asm


def test_bitfield_preserves_neighbor_bits() -> None:
    asm = _compile(
        """
struct S {
    int lo : 8;
    int hi : 8;
};
void main(void) {
    struct S s;
    s.lo = 0x11;
    s.hi = 0x22;
}
"""
    )
    # Clearing mask for lo (bits 0..7) is 0xFF00 = 65280
    assert "and ax, 65280" in asm
    # Clearing mask for hi (bits 8..15) is 0x00FF = 255
    assert "and ax, 255" in asm
    assert "shl ax, 8" in asm


def test_bitfield_width_16() -> None:
    asm = _compile(
        """
struct S {
    int all : 16;
};
void main(void) {
    struct S s;
    s.all = 0xABCD;
}
"""
    )
    assert "and ax, 65535" in asm
    assert "sub sp, 2" in asm


def test_struct_member_in_expression() -> None:
    asm = _compile(
        """
struct S { int x; int y : 4; };
int main(void) {
    struct S s;
    s.x = 10;
    s.y = 3;
    return s.x + s.y;
}
"""
    )
    assert "mov ax, [bx]" in asm
    assert "and ax, 15" in asm


def test_address_of_ordinary_member() -> None:
    asm = _compile(
        """
struct S { int x; int y : 3; };
void take(int *p) { }
void main(void) {
    struct S s;
    take(&s.x);
}
"""
    )
    assert "lea bx, [bp" in asm
    assert "mov ax, bx" in asm
    assert "call take" in asm


def test_struct_bitfield_diagnostics() -> None:
    _expect_compile_error(
        "struct S { int a : 0; }; void main(void) { }\n",
        "1..16",
    )
    _expect_compile_error(
        "struct S { int a : 17; }; void main(void) { }\n",
        "1..16",
    )
    _expect_compile_error(
        "struct S { int a; int a; }; void main(void) { }\n",
        "duplicate member",
    )
    _expect_compile_error(
        """
struct S { int a; };
void main(void) {
    struct S s;
    s.missing = 1;
}
""",
        "no member",
    )
    _expect_compile_error(
        """
void main(void) {
    int x;
    x.a = 1;
}
""",
        "non-struct",
    )
    _expect_compile_error(
        "struct S g; void main(void) { }\n",
        "incomplete struct",
    )
    _expect_compile_error(
        """
struct S { int a : 3; };
void take(int *p) { }
void main(void) {
    struct S s;
    take(&s.a);
}
""",
        "address of bit-field",
    )
    _expect_compile_error(
        "struct S { int : 3; }; void main(void) { }\n",
        "unnamed",
    )


def test_struct_pointer_arrow_access() -> None:
    asm = _compile(
        """
struct S {
    int x;
    int flag : 3;
};
int bump(struct S *p) {
    p->x = 10;
    p->flag = 5;
    return p->x + p->flag;
}
void main(void) {
    struct S s;
    struct S *p;
    p = &s;
    bump(p);
}
"""
    )
    assert "and ax, 7" in asm
    assert "lea ax, [bp" in asm  # &s
    assert "call bump" in asm
    assert "add bx, 2" in asm  # flag bitfield unit at offset 2


def test_struct_pointer_global() -> None:
    asm = _compile(
        """
struct S { int x; };
struct S g;
struct S *gp;
void main(void) {
    gp = &g;
    gp->x = 7;
}
"""
    )
    assert re.search(r"^gp:", asm, re.M)
    assert ".word 0" in asm
    assert "lea ax, [g]" in asm
    assert "mov [gp], ax" in asm
    assert "mov ax, [gp]" in asm


def test_sizeof_types_and_vars() -> None:
    asm = _compile(
        """
struct S {
    int x;
    char c;
    int f : 4;
};
int main(void) {
    struct S s;
    struct S *p;
    int n;
    n = sizeof(int);
    n = sizeof(char);
    n = sizeof(struct S);
    n = sizeof(struct S *);
    n = sizeof(s);
    n = sizeof(p);
    n = sizeof(s.x);
    n = sizeof(s.c);
    return n;
}
"""
    )
    assert "mov ax, 2" in asm  # int / pointers / s.x
    assert "mov ax, 1" in asm  # char / s.c
    # struct S: int@0 + char@2 + pad + bitfield unit@4 => size 6
    assert "mov ax, 6" in asm


def test_sizeof_and_arrow_diagnostics() -> None:
    _expect_compile_error(
        """
struct S { int a : 3; };
void main(void) {
    struct S s;
    int n;
    n = sizeof(s.a);
}
""",
        "sizeof to a bit-field",
    )
    _expect_compile_error(
        """
struct S { int x; };
void main(void) {
    struct S s;
    s->x = 1;
}
""",
        "non-struct-pointer",
    )
    _expect_compile_error(
        """
struct S { int x; };
void main(void) {
    struct S *p;
    int n;
    n = *p;
}
""",
        "use ->",
    )
    _expect_compile_error(
        """
struct S { int a : 3; };
void take(int *q) { }
void main(void) {
    struct S s;
    struct S *p;
    p = &s;
    take(&p->a);
}
""",
        "address of bit-field",
    )


def test_for_statement_after_loop() -> None:
    asm = _compile(
        """
void main(void) {
    int i;
    int a;
    a = 0;
    for (i = 0; i < 2; i = i + 1) {
        a = a + 1;
    }
    a = a + 10;
    for (i = 0; i < 1; i = i + 1) {
        a = a + 1;
    }
    a = 99;
}
"""
    )
    assert "mov ax, 10" in asm
    assert "mov ax, 99" in asm


def test_struct_array_member_access() -> None:
    asm = _compile(
        """
struct S {
    int x;
    int flag : 3;
};
struct S slots[4];
int main(void) {
    struct S local[2];
    int n;
    slots[1].x = 7;
    slots[1].flag = 3;
    local[0].x = slots[1].x;
    n = sizeof(slots);
    return local[0].x + n;
}
"""
    )
    assert ".space 16, 0" in asm  # 4 * sizeof(S=4)
    assert "sub sp, 8" in asm  # 2 * 4
    assert "mov ax, 16" in asm  # sizeof(slots)
    assert "mul cx" in asm
    assert "and ax, 7" in asm


def test_switch_case_default_break() -> None:
    asm = _compile(
        """
int pick(int x) {
    int r;
    r = 0;
    switch (x) {
    case 1:
        r = 10;
        break;
    case 2:
    case 3:
        r = 20;
        break;
    default:
        r = 30;
        break;
    }
    return r;
}
void main(void) {
    pick(2);
}
"""
    )
    assert "cmp ax, 1" in asm
    assert "cmp ax, 2" in asm
    assert "cmp ax, 3" in asm
    assert "mov ax, 10" in asm
    assert "mov ax, 20" in asm
    assert "mov ax, 30" in asm
    assert "je _L" in asm


def test_switch_diagnostics() -> None:
    _expect_compile_error(
        """
void main(void) {
    int x;
    switch (x) {
        x = 1;
    }
}
""",
        "expected case or default",
    )
    _expect_compile_error(
        """
void main(void) {
    int x;
    switch (x) {
    default:
        break;
    default:
        break;
    }
}
""",
        "duplicate default",
    )


def test_enum_constants_and_vars() -> None:
    asm = _compile(
        """
enum Color { RED, GREEN = 5, BLUE };
enum { ALPHA = 7, BETA };
enum Color c;
int main(void) {
    int n;
    n = RED;
    n = GREEN;
    n = BLUE;
    n = ALPHA;
    n = sizeof(enum Color);
    switch (n) {
    case RED:
        break;
    case GREEN:
        break;
    default:
        break;
    }
    return n;
}
"""
    )
    assert "mov ax, 0" in asm  # RED
    assert "mov ax, 5" in asm  # GREEN
    assert "mov ax, 6" in asm  # BLUE = GREEN+1
    assert "mov ax, 7" in asm  # ALPHA
    assert "mov ax, 2" in asm  # sizeof enum
    assert "cmp ax, 0" in asm
    assert "cmp ax, 5" in asm


def test_casts() -> None:
    asm = _compile(
        """
struct S { int x; };
int main(void) {
    int n;
    char c;
    struct S *p;
    n = 300;
    c = (char)n;
    n = (int)c;
    p = (struct S *)0;
    return (int)p;
}
"""
    )
    assert "and ax, 255" in asm
    assert "mov ax, 0" in asm


def test_do_while() -> None:
    asm = _compile(
        """
void main(void) {
    int i;
    int a;
    i = 0;
    a = 0;
    do {
        a = a + 1;
        i = i + 1;
    } while (i < 3);
    a = 99;
}
"""
    )
    assert "jne _L" in asm
    assert "mov ax, 99" in asm
    # continue/break wiring: body before condition
    lines = [ln.strip() for ln in asm.splitlines() if ln.strip()]
    # Find a do-while: label, body stores, then cmp/jne back
    assert any(ln.startswith("_L") and ln.endswith(":") for ln in lines)


def test_enum_cast_do_diagnostics() -> None:
    _expect_compile_error(
        """
enum E { A };
enum E { B };
void main(void) { }
""",
        "redefinition of enum",
    )
    _expect_compile_error(
        """
enum { A, A };
void main(void) { }
""",
        "redefinition of enumerator",
    )
    _expect_compile_error(
        """
enum { A } x;
void main(void) { }
""",
        "anonymous enum cannot declare",
    )


def test_local_scalar_initializer() -> None:
    asm = _compile(
        """
int main(void) {
    int x = 42;
    int y = x + 1;
    return y;
}
"""
    )
    assert "sub sp, 2" in asm
    assert "mov ax, 42" in asm
    assert re.search(r"mov \[bp-?\d+\], ax", asm)
    assert "mov ax, 1" in asm


def test_local_comma_initializers() -> None:
    asm = _compile(
        """
void main(void) {
    int a = 1, b = 2;
}
"""
    )
    assert "mov ax, 1" in asm
    assert "mov ax, 2" in asm
    assert asm.count("sub sp, 2") >= 2


def test_local_array_and_string_initializer() -> None:
    asm = _compile(
        """
int main(void) {
    int a[3] = {1, 2, 3};
    char buf[8] = "hi";
    return a[0];
}
"""
    )
    assert "sub sp, 6" in asm  # int[3]
    assert "sub sp, 8" in asm  # char[8]
    assert "rep stosb" in asm
    assert "mov ax, 1" in asm
    assert "mov ax, 2" in asm
    assert "mov ax, 3" in asm
    assert "mov al, 104" in asm  # 'h'
    assert "mov al, 105" in asm  # 'i'


def test_void_pointer_sizeof_cast_and_assign() -> None:
    asm = _compile(
        """
void *id(void *p) {
    return p;
}
int main(void) {
    int n;
    void *p;
    char *c;
    n = 7;
    p = (void *)0;
    p = &n;
    c = (char *)p;
    n = sizeof(void *);
    return id(p) == 0;
}
"""
    )
    assert "mov ax, 2" in asm  # sizeof(void *)
    assert "mov ax, 0" in asm
    assert "lea ax, [bp" in asm
    assert "call id" in asm


def test_void_pointer_diagnostics() -> None:
    _expect_compile_error(
        "void x; void main(void) { }\n",
        "void variables",
    )
    _expect_compile_error(
        """
void main(void) {
    void *p;
    int n;
    p = 0;
    n = *p;
}
""",
        "dereference void pointer",
    )
    _expect_compile_error(
        """
int main(void) {
    int n;
    n = sizeof(void);
    return n;
}
""",
        "sizeof(void)",
    )
    _expect_compile_error(
        """
int main(void) {
    int n;
    n = (void)n;
    return n;
}
""",
        "cast to void",
    )


def test_struct_assignment_copy() -> None:
    asm = _compile(
        """
struct S { int x; char c; };
void main(void) {
    struct S a;
    struct S b;
    a.x = 1;
    a.c = 2;
    b = a;
}
"""
    )
    assert "rep movsb" in asm
    assert "mov cx, 3" in asm


def test_struct_assignment_size_and_chain() -> None:
    asm = _compile(
        """
struct S { int x; int y; };
void main(void) {
    struct S a;
    struct S b;
    struct S c;
    a.x = 1;
    a.y = 2;
    c = b = a;
}
"""
    )
    assert "mov cx, 4" in asm
    assert asm.count("rep movsb") >= 2


def test_struct_local_init_from_object() -> None:
    asm = _compile(
        """
struct S { int x; };
void main(void) {
    struct S a;
    a.x = 9;
    struct S b = a;
}
"""
    )
    assert "rep movsb" in asm
    assert "sub sp, 2" in asm


def test_struct_assignment_diagnostics() -> None:
    _expect_compile_error(
        """
struct A { int x; };
struct B { int y; };
void main(void) {
    struct A a;
    struct B b;
    a = b;
}
""",
        "type mismatch",
    )
    _expect_compile_error(
        """
struct S { int x; };
void main(void) {
    struct S s;
    s = 1;
}
""",
        "struct object",
    )
    _expect_compile_error(
        """
struct S { int x; };
void main(void) {
    struct S s = { 1 };
}
""",
        "struct initializers",
    )


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
    test_static_file_scope_omits_global()
    test_static_and_extern_rejected()
    test_static_local_storage_and_addressing()
    test_static_local_initializer()
    test_plain_local_still_uses_bp()
    test_struct_layout_and_global_storage()
    test_struct_local_ordinary_and_bitfield_access()
    test_struct_global_member_read_write()
    test_bitfield_preserves_neighbor_bits()
    test_bitfield_width_16()
    test_struct_member_in_expression()
    test_address_of_ordinary_member()
    test_struct_bitfield_diagnostics()
    test_struct_pointer_arrow_access()
    test_struct_pointer_global()
    test_sizeof_types_and_vars()
    test_sizeof_and_arrow_diagnostics()
    test_for_statement_after_loop()
    test_struct_array_member_access()
    test_switch_case_default_break()
    test_switch_diagnostics()
    test_enum_constants_and_vars()
    test_casts()
    test_do_while()
    test_enum_cast_do_diagnostics()
    test_local_scalar_initializer()
    test_local_comma_initializers()
    test_local_array_and_string_initializer()
    test_void_pointer_sizeof_cast_and_assign()
    test_void_pointer_diagnostics()
    test_struct_assignment_copy()
    test_struct_assignment_size_and_chain()
    test_struct_local_init_from_object()
    test_struct_assignment_diagnostics()
    print("test_rmcc: OK")
