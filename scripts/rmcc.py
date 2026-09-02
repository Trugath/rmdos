#!/usr/bin/env python3
"""
rmcc: Small-C compiler for rmDOS.
Compiles a C subset to 8088 GAS assembly.
Usage: python3 -m scripts.rmcc [options] input.c -o output.s
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from scripts.rmcc_preprocess import default_include_dirs, preprocess_includes

# --- Token types ---
T_INT, T_CHAR, T_VOID = "INT", "CHAR", "VOID"
T_STRUCT, T_ENUM, T_SIZEOF = "STRUCT", "ENUM", "SIZEOF"
T_EXTERN, T_STATIC = "EXTERN", "STATIC"
T_IF, T_ELSE, T_WHILE, T_FOR, T_DO, T_RETURN, T_ASM, T_BREAK, T_CONTINUE = (
    "IF", "ELSE", "WHILE", "FOR", "DO", "RETURN", "ASM", "BREAK", "CONTINUE",
)
T_SWITCH, T_CASE, T_DEFAULT = "SWITCH", "CASE", "DEFAULT"
T_IDENT, T_NUMBER, T_STRING = "IDENT", "NUMBER", "STRING"
T_LBRACE, T_RBRACE, T_LPAREN, T_RPAREN, T_LBRACKET, T_RBRACKET = "LBRACE", "RBRACE", "LPAREN", "RPAREN", "LBRACKET", "RBRACKET"
T_SEMI, T_COMMA, T_DOT, T_ARROW = "SEMI", "COMMA", "DOT", "ARROW"
T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT = "PLUS", "MINUS", "STAR", "SLASH", "PERCENT"
T_EQ, T_NE, T_LT, T_LE, T_GT, T_GE = "EQ", "NE", "LT", "LE", "GT", "GE"
T_AND, T_OR, T_XOR, T_NOT = "AND", "OR", "XOR", "NOT"
T_LAND, T_LOR, T_LNOT = "LAND", "LOR", "LNOT"
T_SHL, T_SHR = "SHL", "SHR"
T_ASSIGN = "ASSIGN"
T_QUESTION, T_COLON = "QUESTION", "COLON"
T_PREPROC = "PREPROC"  # #define
T_MACRO_END = "MACRO_END"  # internal: pop hidden macro after expansion
T_EOF = "EOF"

KEYWORDS = {
    "int": T_INT, "char": T_CHAR, "void": T_VOID, "struct": T_STRUCT, "enum": T_ENUM,
    "sizeof": T_SIZEOF,
    "extern": T_EXTERN, "static": T_STATIC,
    "if": T_IF, "else": T_ELSE, "while": T_WHILE, "for": T_FOR, "do": T_DO,
    "switch": T_SWITCH, "case": T_CASE, "default": T_DEFAULT,
    "return": T_RETURN, "asm": T_ASM, "break": T_BREAK, "continue": T_CONTINUE,
}


@dataclass
class StructMember:
    """One ordinary or bit-field member of a tagged struct."""
    name: str
    typ: str  # "int" or "char"
    byte_offset: int
    is_bitfield: bool = False
    bit_offset: int = 0  # LSB-first bit index within the 16-bit unit
    bit_width: int = 0

    @property
    def bit_mask(self) -> int:
        """Mask of the field bits within its storage word (already shifted)."""
        if not self.is_bitfield:
            return 0
        return ((1 << self.bit_width) - 1) << self.bit_offset

    @property
    def value_mask(self) -> int:
        """Mask of the field value in isolation (unshifted)."""
        if not self.is_bitfield:
            return 0
        return (1 << self.bit_width) - 1


@dataclass
class StructType:
    tag: str
    members: dict[str, StructMember] = field(default_factory=dict)
    size: int = 0
    complete: bool = False


@dataclass
class Token:
    kind: str
    value: Any
    line: int
    col: int

    def __repr__(self):
        return f"Token({self.kind}, {self.value!r}, {self.line}:{self.col})"


@dataclass
class Macro:
    """Object-like if params is None; function-like otherwise (possibly zero params)."""
    params: list[str] | None
    body: list[Token]


class CompileError(Exception):
    def __init__(self, msg: str, line: int, col: int):
        self.msg = msg
        self.line = line
        self.col = col
        super().__init__(f"{line}:{col}: {msg}")


# --- Lexer ---
class Lexer:
    def __init__(self, source: str, filename: str = "<input>", macros: dict[str, Macro] | None = None):
        self.source = source
        self.filename = filename
        self.macros: dict[str, Macro] = macros or {}
        self.pos = 0
        self.line = 1
        self.col = 1
        self._pending: list[Token] = []  # for macro expansion
        self._hidden: set[str] = set()  # macros disabled during their own expansion
        self._expand = True  # False while reading #define body / macro args

    def _peek(self) -> str:
        if self.pos >= len(self.source):
            return "\0"
        return self.source[self.pos]

    def _advance(self) -> str:
        if self.pos >= len(self.source):
            return "\0"
        c = self.source[self.pos]
        self.pos += 1
        if c == "\n":
            self.line += 1
            self.col = 1
        else:
            self.col += 1
        return c

    def _skip_whitespace_and_comments(self):
        while True:
            c = self._peek()
            if c in " \t\n\r":
                self._advance()
            elif c == "/" and self.pos + 1 < len(self.source) and self.source[self.pos + 1] == "*":
                self._advance()
                self._advance()
                while self._peek() != "\0" and not (self._peek() == "*" and self.pos + 1 < len(self.source) and self.source[self.pos + 1] == "/"):
                    self._advance()
                if self._peek() != "\0":
                    self._advance()
                    self._advance()
            elif c == "/" and self.pos + 1 < len(self.source) and self.source[self.pos + 1] == "/":
                while self._peek() != "\0" and self._peek() != "\n":
                    self._advance()
            else:
                break

    def _read_identifier(self) -> str:
        start = self.pos
        while self._peek().isalnum() or self._peek() == "_":
            self._advance()
        return self.source[start:self.pos]

    def _read_number(self) -> tuple[int, int]:
        start = self.pos
        base = 10
        if self._peek() == "0" and self.pos + 1 < len(self.source):
            n = self.source[self.pos + 1]
            if n == "x" or n == "X":
                base = 16
                self._advance()
                self._advance()
            elif n.isdigit():
                base = 8
        num_str = ""
        if base == 16:
            while self._peek() in "0123456789abcdefABCDEF":
                num_str += self._advance()
        else:
            while self._peek().isdigit():
                num_str += self._advance()
        return int(num_str or "0", base), start

    def _read_string(self) -> str:
        quote = self._peek()
        self._advance()  # opening quote
        result = []
        while self._peek() != quote and self._peek() != "\0":
            c = self._advance()
            if c == "\\":
                n = self._advance()
                if n == "n":
                    result.append("\n")
                elif n == "r":
                    result.append("\r")
                elif n == "t":
                    result.append("\t")
                elif n == "0":
                    result.append("\0")
                elif n == "\\" or n == '"' or n == "'":
                    result.append(n)
                else:
                    result.append(n)
            else:
                result.append(c)
        if self._peek() == quote:
            self._advance()
        return "".join(result)

    def push_back(self, t: Token):
        self._pending.insert(0, t)

    def _skip_hspace(self):
        """Skip horizontal whitespace and comments, but not newlines (for #define)."""
        while True:
            c = self._peek()
            if c in " \t\r":
                self._advance()
            elif c == "/" and self.pos + 1 < len(self.source) and self.source[self.pos + 1] == "*":
                self._advance()
                self._advance()
                while self._peek() != "\0" and not (
                    self._peek() == "*"
                    and self.pos + 1 < len(self.source)
                    and self.source[self.pos + 1] == "/"
                ):
                    self._advance()
                if self._peek() != "\0":
                    self._advance()
                    self._advance()
            elif c == "/" and self.pos + 1 < len(self.source) and self.source[self.pos + 1] == "/":
                while self._peek() != "\0" and self._peek() != "\n":
                    self._advance()
            else:
                break

    def _next_raw(self) -> Token:
        """Next token without macro expansion (for #define bodies and macro args)."""
        saved = self._expand
        self._expand = False
        try:
            return self.next_token()
        finally:
            self._expand = saved

    def _read_define(self, line: int, col: int) -> None:
        """Parse #define NAME ... or #define NAME(params) ... through end of line."""
        name = self._read_identifier()
        if not name:
            raise CompileError("expected macro name after #define", line, col)
        params = None
        # Function-like only if '(' immediately follows the name (no space).
        if self._peek() == "(":
            self._advance()
            params = []
            self._skip_hspace()
            if self._peek() != ")":
                while True:
                    self._skip_hspace()
                    if not (self._peek().isalpha() or self._peek() == "_"):
                        raise CompileError("expected macro parameter name", self.line, self.col)
                    params.append(self._read_identifier())
                    self._skip_hspace()
                    if self._peek() == ",":
                        self._advance()
                        continue
                    break
            if self._peek() != ")":
                raise CompileError("expected ')' after macro parameter list", self.line, self.col)
            self._advance()
        self._skip_hspace()
        body = []
        while self._peek() not in "\0\n":
            self._skip_hspace()
            if self._peek() in "\0\n":
                break
            body.append(self._next_raw())
        self.macros[name] = Macro(params, body)

    def _peek_is_lparen(self) -> bool:
        """True if the next token (after whitespace) is '(' — for function-like invoke."""
        if self._pending:
            return self._pending[0].kind == T_LPAREN
        save_pos, save_line, save_col = self.pos, self.line, self.col
        self._skip_whitespace_and_comments()
        is_lp = self._peek() == "("
        self.pos, self.line, self.col = save_pos, save_line, save_col
        return is_lp

    def _collect_macro_args(self, line: int, col: int) -> list:
        """Consume '(...)' and return argument token lists (commas at paren depth 0)."""
        if self._pending and self._pending[0].kind == T_LPAREN:
            self._pending.pop(0)
        else:
            self._skip_whitespace_and_comments()
            if self._peek() != "(":
                raise CompileError("expected '(' for function-like macro", line, col)
            self._advance()

        args = []
        cur = []
        depth = 0
        while True:
            t = self._next_raw()
            if t.kind == T_EOF:
                raise CompileError("unterminated macro argument list", line, col)
            if t.kind == T_LPAREN:
                depth += 1
                cur.append(t)
            elif t.kind == T_RPAREN:
                if depth == 0:
                    args.append(cur)
                    break
                depth -= 1
                cur.append(t)
            elif t.kind == T_COMMA and depth == 0:
                args.append(cur)
                cur = []
            else:
                cur.append(t)
        return args

    def _substitute_macro(self, macro: Macro, args, line: int, col: int) -> list:
        if macro.params is None:
            return list(macro.body)
        if args is None:
            raise CompileError("function-like macro requires arguments", line, col)
        # Zero-parameter macro: FOO() yields args [[]] from collector — treat as no args.
        if len(macro.params) == 0:
            if args == [[]]:
                args = []
            if len(args) != 0:
                raise CompileError(
                    f"macro expects 0 arguments, got {len(args)}", line, col
                )
        elif len(args) != len(macro.params):
            raise CompileError(
                f"macro expects {len(macro.params)} arguments, got {len(args)}",
                line,
                col,
            )
        param_map = {p: args[i] for i, p in enumerate(macro.params)}
        out = []
        for t in macro.body:
            if t.kind == T_IDENT and t.value in param_map:
                out.extend(param_map[t.value])
            else:
                out.append(t)
        return out

    def _push_expansion(self, name: str, tokens: list) -> None:
        self._hidden.add(name)
        self._pending = list(tokens) + [Token(T_MACRO_END, name, 0, 0)] + self._pending

    def _expand_ident(self, ident: str, line: int, col: int) -> Token:
        macro = self.macros[ident]
        if macro.params is None:
            self._push_expansion(ident, self._substitute_macro(macro, None, line, col))
            return self.next_token()
        # Function-like: only expand when followed by '('; otherwise leave as identifier.
        if not self._peek_is_lparen():
            return Token(T_IDENT, ident, line, col)
        args = self._collect_macro_args(line, col)
        self._push_expansion(ident, self._substitute_macro(macro, args, line, col))
        return self.next_token()

    def next_token(self) -> Token:
        if self._pending:
            t = self._pending.pop(0)
            if t.kind == T_MACRO_END:
                self._hidden.discard(t.value)
                return self.next_token()
            if (
                self._expand
                and t.kind == T_IDENT
                and t.value not in KEYWORDS
                and t.value in self.macros
                and t.value not in self._hidden
            ):
                return self._expand_ident(t.value, t.line, t.col)
            return t

        self._skip_whitespace_and_comments()
        line, col = self.line, self.col
        c = self._peek()

        if c == "\0":
            return Token(T_EOF, None, line, col)

        # Preprocessor
        if c == "#" and self.col == 1:
            self._advance()
            directive = self._read_identifier()
            self._skip_hspace()
            if directive == "define":
                self._read_define(line, col)
            else:
                while self._peek() not in "\0\n":
                    self._advance()
            return self.next_token()

        # Identifier or keyword
        if c.isalpha() or c == "_":
            ident = self._read_identifier()
            if ident in KEYWORDS:
                return Token(KEYWORDS[ident], ident, line, col)
            if self._expand and ident in self.macros and ident not in self._hidden:
                return self._expand_ident(ident, line, col)
            return Token(T_IDENT, ident, line, col)

        # Number
        if c.isdigit():
            val, _ = self._read_number()
            return Token(T_NUMBER, val, line, col)

        # String and character constants.  A character constant is an integer
        # expression in C, while a double-quoted literal decays to a pointer.
        # Treating both as T_STRING made comparisons such as c == ' ' compare
        # against the address of a generated string, breaking tokenization.
        if c == "'":
            s = self._read_string()
            if len(s) != 1:
                raise CompileError("character constant must contain one character", line, col)
            return Token(T_NUMBER, ord(s), line, col)
        if c == '"':
            s = self._read_string()
            return Token(T_STRING, s, line, col)

        # Two-char operators
        if c == "=" and self.pos + 1 < len(self.source) and self.source[self.pos + 1] == "=":
            self._advance()
            self._advance()
            return Token(T_EQ, "==", line, col)
        if c == "!" and self.pos + 1 < len(self.source) and self.source[self.pos + 1] == "=":
            self._advance()
            self._advance()
            return Token(T_NE, "!=", line, col)
        if c == "<" and self.pos + 1 < len(self.source) and self.source[self.pos + 1] == "=":
            self._advance()
            self._advance()
            return Token(T_LE, "<=", line, col)
        if c == ">" and self.pos + 1 < len(self.source) and self.source[self.pos + 1] == "=":
            self._advance()
            self._advance()
            return Token(T_GE, ">=", line, col)
        if c == "&" and self.pos + 1 < len(self.source) and self.source[self.pos + 1] == "&":
            self._advance()
            self._advance()
            return Token(T_LAND, "&&", line, col)
        if c == "|" and self.pos + 1 < len(self.source) and self.source[self.pos + 1] == "|":
            self._advance()
            self._advance()
            return Token(T_LOR, "||", line, col)
        if c == "<" and self.pos + 1 < len(self.source) and self.source[self.pos + 1] == "<":
            self._advance()
            self._advance()
            return Token(T_SHL, "<<", line, col)
        if c == ">" and self.pos + 1 < len(self.source) and self.source[self.pos + 1] == ">":
            self._advance()
            self._advance()
            return Token(T_SHR, ">>", line, col)
        if c == "-" and self.pos + 1 < len(self.source) and self.source[self.pos + 1] == ">":
            self._advance()
            self._advance()
            return Token(T_ARROW, "->", line, col)

        # Single char
        single = {
            "{": T_LBRACE, "}": T_RBRACE, "(": T_LPAREN, ")": T_RPAREN,
            "[": T_LBRACKET, "]": T_RBRACKET, ";": T_SEMI, ",": T_COMMA, ".": T_DOT,
            "+": T_PLUS, "-": T_MINUS, "*": T_STAR, "/": T_SLASH, "%": T_PERCENT,
            "<": T_LT, ">": T_GT, "=": T_ASSIGN,
            "&": T_AND, "|": T_OR, "^": T_XOR, "!": T_LNOT, "~": T_NOT,
            "?": T_QUESTION, ":": T_COLON,
        }
        self._advance()
        if c in single:
            return Token(single[c], c, line, col)
        raise CompileError(f"unexpected character '{c}'", line, col)



# --- Code generator ---
class CodeGen:
    def __init__(self):
        self.lines: list[str] = []
        self.label_num = 0
        self.string_num = 0
        self.strings: list[tuple[str, str]] = []  # (label, content)

    def emit(self, s: str):
        self.lines.append(s)

    def new_label(self) -> str:
        n = self.label_num
        self.label_num += 1
        return f"_L{n}"

    def add_string(self, s: str) -> str:
        label = f"_s{self.string_num}"
        self.string_num += 1
        # Escape for .asciz: backslash and quote
        escaped = s.replace("\\", "\\\\").replace('"', '\\"')
        self.strings.append((label, s))
        return label


# --- Compiler (parser + codegen) ---
class Compiler:
    def __init__(
        self,
        source: str,
        filename: str = "<input>",
        module_name: str | None = None,
        com_entry: bool = False,
        exe_entry: bool = False,
        overlay_entry: bool = False,
    ):
        self.lexer = Lexer(source, filename)
        self.gen = CodeGen()
        self.module_name = module_name
        self.com_entry = com_entry
        self.exe_entry = exe_entry
        self.overlay_entry = overlay_entry
        self.cur_token: Token | None = None
        self.globals: dict[str, tuple[str, int]] = {}  # name -> (type, size) size=0 for scalar
        self.locals: dict[str, tuple[str, int]] = {}  # name -> (type, bp_offset)
        self.params: list[tuple[str, str]] = []  # (name, type)
        self.structs: dict[str, StructType] = {}  # tag -> StructType
        self.enum_tags: set[str] = set()
        self.enumerators: dict[str, int] = {}  # name -> constant value
        self.local_offset = 0
        self.last_primary_type = "int"
        self.include_paths: list[Path] = []
        self.break_labels: list[str] = []
        self.continue_labels: list[str] = []
        self.arrays: set[str] = set()  # names that decay to pointers
        self.array_lengths: dict[str, int] = {}  # element counts for arrays
        self.static_locals: dict[str, str] = {}  # C name -> asm label (function-local static)
        self.current_func: str | None = None
        self._static_local_seq = 0

    def _label_for(self, name: str) -> str:
        """Asm symbol for a C name (mangled for function-local static)."""
        return self.static_locals.get(name, name)

    def _advance(self) -> Token:
        self.cur_token = self.lexer.next_token()
        return self.cur_token

    def _expect(self, kind: str) -> Token:
        t = self.cur_token
        if t.kind != kind:
            raise CompileError(f"expected {kind}, got {t.kind}", t.line, t.col)
        self._advance()
        return t

    def _at(self, kind: str) -> bool:
        return self.cur_token is not None and self.cur_token.kind == kind

    def _peek_next(self) -> Token:
        t = self.lexer.next_token()
        self.lexer.push_back(t)
        return t

    def _type_spec(self) -> str | None:
        if self._at(T_INT):
            self._advance()
            return "int"
        if self._at(T_CHAR):
            self._advance()
            return "char"
        if self._at(T_VOID):
            self._advance()
            return "void"
        return None

    def _is_struct_type(self, typ: str) -> bool:
        return typ.startswith("struct ") and not typ.endswith("*")

    def _is_struct_ptr(self, typ: str) -> bool:
        return typ.startswith("struct ") and typ.endswith("*")

    def _struct_tag(self, typ: str) -> str:
        s = typ[len("struct "):]
        if s.endswith("*"):
            s = s[:-1]
        return s

    def _struct_obj_type(self, typ: str) -> str:
        """Return object type 'struct Tag' from 'struct Tag' or 'struct Tag*'."""
        if typ.endswith("*"):
            return typ[:-1]
        return typ

    def _type_size(self, typ: str) -> int:
        if typ.endswith("*") or typ == "int" or typ.startswith("enum "):
            return 2
        if typ == "char":
            return 1
        if typ == "void":
            return 0
        if self._is_struct_type(typ):
            tag = self._struct_tag(typ)
            st = self.structs.get(tag)
            if st is None or not st.complete:
                return 0
            return st.size
        return 0

    def _apply_pointer_star(self, t: str) -> str:
        """Consume one trailing '*' (including void*)."""
        if self._at(T_STAR):
            self._advance()
            t = t + "*"
        return t

    def _reject_void_object(self, t: str, line: int, col: int) -> None:
        if t == "void":
            raise CompileError("void variables are not allowed", line, col)

    def _is_enum_type(self, typ: str) -> bool:
        return typ.startswith("enum ")

    def _is_cast_type_start(self) -> bool:
        return (
            self._at(T_INT)
            or self._at(T_CHAR)
            or self._at(T_VOID)
            or self._at(T_STRUCT)
            or self._at(T_ENUM)
        )

    def _require_complete_struct(self, typ: str, line: int, col: int) -> StructType:
        if self._is_struct_ptr(typ):
            typ = self._struct_obj_type(typ)
        if not self._is_struct_type(typ):
            raise CompileError(f"expected struct type, got '{typ}'", line, col)
        tag = self._struct_tag(typ)
        st = self.structs.get(tag)
        if st is None or not st.complete:
            raise CompileError(f"incomplete struct '{tag}'", line, col)
        return st

    def _layout_struct_members(
        self,
        tag: str,
        member_specs: list[tuple[str, str, int | None, int, int]],
    ) -> StructType:
        """Build StructType from (name, typ, bit_width_or_None, line, col) specs."""
        members: dict[str, StructMember] = {}
        offset = 0
        bit_unit_start: int | None = None
        bit_pos = 0

        def close_bit_unit() -> None:
            nonlocal offset, bit_unit_start, bit_pos
            if bit_unit_start is not None:
                offset = bit_unit_start + 2
                bit_unit_start = None
                bit_pos = 0

        for name, typ, width, line, col in member_specs:
            if name in members:
                raise CompileError(f"duplicate member '{name}' in struct '{tag}'", line, col)
            if width is not None:
                if typ != "int":
                    raise CompileError("bit-fields must have type int", line, col)
                if width < 1 or width > 16:
                    raise CompileError(
                        f"bit-field width must be 1..16 (got {width})", line, col
                    )
                if bit_unit_start is None:
                    if offset % 2:
                        offset += 1
                    bit_unit_start = offset
                    bit_pos = 0
                if bit_pos + width > 16:
                    close_bit_unit()
                    if offset % 2:
                        offset += 1
                    bit_unit_start = offset
                    bit_pos = 0
                members[name] = StructMember(
                    name=name,
                    typ=typ,
                    byte_offset=bit_unit_start,
                    is_bitfield=True,
                    bit_offset=bit_pos,
                    bit_width=width,
                )
                bit_pos += width
                if bit_pos == 16:
                    close_bit_unit()
            else:
                close_bit_unit()
                if typ == "int":
                    if offset % 2:
                        offset += 1
                    members[name] = StructMember(name=name, typ=typ, byte_offset=offset)
                    offset += 2
                elif typ == "char":
                    members[name] = StructMember(name=name, typ=typ, byte_offset=offset)
                    offset += 1
                else:
                    raise CompileError(
                        f"unsupported struct member type '{typ}'", line, col
                    )

        close_bit_unit()
        return StructType(tag=tag, members=members, size=offset, complete=True)

    def _parse_struct_type(self) -> str:
        """Parse 'struct Tag' or 'struct Tag { ... }'. Returns 'struct Tag'."""
        self._expect(T_STRUCT)
        if not self._at(T_IDENT):
            raise CompileError("expected struct tag", self.cur_token.line, self.cur_token.col)
        tag_tok = self.cur_token
        tag = tag_tok.value
        self._advance()
        if self._at(T_LBRACE):
            if tag in self.structs and self.structs[tag].complete:
                raise CompileError(f"redefinition of struct '{tag}'", tag_tok.line, tag_tok.col)
            self._advance()
            specs: list[tuple[str, str, int | None, int, int]] = []
            while not self._at(T_RBRACE) and not self._at(T_EOF):
                mtyp = self._type_spec()
                if mtyp is None:
                    raise CompileError(
                        "expected member type", self.cur_token.line, self.cur_token.col
                    )
                if mtyp == "void":
                    raise CompileError("void struct members are not allowed", self.cur_token.line, self.cur_token.col)
                if self._at(T_STAR):
                    raise CompileError(
                        "struct pointers are not supported", self.cur_token.line, self.cur_token.col
                    )
                if not self._at(T_IDENT):
                    raise CompileError(
                        "anonymous / unnamed bit-fields are not supported",
                        self.cur_token.line,
                        self.cur_token.col,
                    )
                mname_tok = self.cur_token
                mname = mname_tok.value
                self._advance()
                width: int | None = None
                if self._at(T_COLON):
                    self._advance()
                    if not self._at(T_NUMBER):
                        raise CompileError(
                            "expected bit-field width", self.cur_token.line, self.cur_token.col
                        )
                    width = int(self.cur_token.value)
                    self._advance()
                if self._at(T_LBRACKET):
                    raise CompileError(
                        "struct member arrays are not supported",
                        self.cur_token.line,
                        self.cur_token.col,
                    )
                specs.append((mname, mtyp, width, mname_tok.line, mname_tok.col))
                self._expect(T_SEMI)
            self._expect(T_RBRACE)
            self.structs[tag] = self._layout_struct_members(tag, specs)
        elif tag not in self.structs:
            # Incomplete tag reference; completeness checked at allocation/use.
            self.structs[tag] = StructType(tag=tag, complete=False)
        return f"struct {tag}"

    def _parse_enum_type(self) -> str:
        """Parse 'enum Tag', 'enum Tag { ... }', or 'enum { ... }'. Returns type string."""
        self._expect(T_ENUM)
        tag: str | None = None
        if self._at(T_IDENT):
            tag = self.cur_token.value
            self._advance()
        if self._at(T_LBRACE):
            self._advance()
            next_val = 0
            saw_member = False
            while not self._at(T_RBRACE) and not self._at(T_EOF):
                if not self._at(T_IDENT):
                    raise CompileError(
                        "expected enumerator name", self.cur_token.line, self.cur_token.col
                    )
                name_tok = self.cur_token
                name = name_tok.value
                self._advance()
                if name in self.enumerators:
                    raise CompileError(
                        f"redefinition of enumerator '{name}'", name_tok.line, name_tok.col
                    )
                if name in self.globals or name in self.locals:
                    raise CompileError(
                        f"enumerator '{name}' conflicts with variable",
                        name_tok.line,
                        name_tok.col,
                    )
                if self._at(T_ASSIGN):
                    self._advance()
                    neg = False
                    if self._at(T_MINUS):
                        self._advance()
                        neg = True
                    if self._at(T_NUMBER):
                        val = int(self.cur_token.value)
                        self._advance()
                    elif self._at(T_IDENT) and self.cur_token.value in self.enumerators:
                        val = self.enumerators[self.cur_token.value]
                        self._advance()
                    else:
                        raise CompileError(
                            "enumerator value must be an integer constant",
                            self.cur_token.line,
                            self.cur_token.col,
                        )
                    if neg:
                        val = -val
                    val &= 0xFFFF
                else:
                    val = next_val & 0xFFFF
                self.enumerators[name] = val
                next_val = (val + 1) & 0xFFFF
                saw_member = True
                if self._at(T_COMMA):
                    self._advance()
                    continue
                break
            if not saw_member:
                raise CompileError("empty enum is not supported", self.cur_token.line, self.cur_token.col)
            self._expect(T_RBRACE)
            if tag is not None:
                if tag in self.enum_tags:
                    raise CompileError(
                        f"redefinition of enum '{tag}'", self.cur_token.line, self.cur_token.col
                    )
                self.enum_tags.add(tag)
                return f"enum {tag}"
            return "enum"
        if tag is None:
            raise CompileError("expected enum tag or '{'", self.cur_token.line, self.cur_token.col)
        self.enum_tags.add(tag)
        return f"enum {tag}"

    def _parse_decl_type(self) -> str | None:
        """Parse a type specifier (int/char/void/struct/enum). May define a struct/enum."""
        if self._at(T_STRUCT):
            return self._parse_struct_type()
        if self._at(T_ENUM):
            return self._parse_enum_type()
        return self._type_spec()

    def _parse_type_and_name(self) -> tuple[str, str, int]:  # (type, name, array_size)
        t = self._type_spec()
        if t is None:
            return None, None, 0
        name = self._expect(T_IDENT).value
        size = 0
        if self._at(T_LBRACKET):
            self._advance()
            n = self._expect(T_NUMBER).value
            size = int(n)
            self._expect(T_RBRACKET)
        return t, name, size

    def _lookup(self, name: str) -> tuple[str, int]:  # (type, offset_or_size)
        if name in self.locals:
            return self.locals[name]
        if name in self.static_locals:
            return self.globals[self.static_locals[name]]
        if name in self.globals:
            return self.globals[name]
        return None, 0

    def _is_local(self, name: str) -> bool:
        return name in self.locals

    def _align_local(self, align: int = 2) -> None:
        rem = self.local_offset % align
        if rem:
            pad = align - rem
            self.gen.emit(f"    sub sp, {pad}")
            self.local_offset += pad

    def _emit_member_addr_bx(self, base_name: str, member: StructMember) -> None:
        """Emit address of member storage (byte/word unit) into BX from a named object."""
        off = member.byte_offset
        if self._is_local(base_name):
            _, base_bp = self.locals[base_name]
            total = base_bp + off
            self.gen.emit(f"    lea bx, [bp{total:+d}]")
        else:
            if off == 0:
                self.gen.emit(f"    lea bx, [{self._label_for(base_name)}]")
            else:
                self.gen.emit(f"    lea bx, [{self._label_for(base_name)}+{off}]")

    def _emit_ptr_member_addr_bx(self, member: StructMember) -> None:
        """AX holds struct pointer; emit member storage address into BX."""
        self.gen.emit("    mov bx, ax")
        if member.byte_offset:
            self.gen.emit(f"    add bx, {member.byte_offset}")

    def _emit_struct_array_elem_addr_bx(self, name: str, typ: str) -> None:
        """AX holds element index; emit address of name[index] into BX."""
        st = self._require_complete_struct(typ, self.cur_token.line, self.cur_token.col)
        sz = st.size
        _t, off = self._lookup(name)
        self.gen.emit("    push ax")
        if self._is_local(name):
            self.gen.emit(f"    lea bx, [bp{off:+d}]")
        else:
            self.gen.emit(f"    lea bx, [{self._label_for(name)}]")
        self.gen.emit("    pop ax")
        if sz == 0:
            pass
        elif sz == 1:
            pass
        else:
            self.gen.emit(f"    mov cx, {sz}")
            self.gen.emit("    mul cx")
        self.gen.emit("    add bx, ax")

    def _emit_load_scalar(self, name: str) -> None:
        """Load a scalar/pointer variable's value into AX."""
        typ, off = self._lookup(name)
        if typ is None:
            raise CompileError(f"undefined identifier '{name}'", self.cur_token.line, self.cur_token.col)
        if self._is_local(name):
            self.gen.emit(f"    mov ax, [bp{off:+d}]")
        else:
            self.gen.emit(f"    mov ax, [{self._label_for(name)}]")
        self.last_primary_type = typ

    def _emit_load_member_at_bx(self, member: StructMember) -> None:
        """Load member whose storage unit address is already in BX."""
        if member.is_bitfield:
            self.gen.emit("    mov ax, [bx]")
            if member.bit_offset:
                self.gen.emit(f"    shr ax, {member.bit_offset}")
            self.gen.emit(f"    and ax, {member.value_mask}")
        elif member.typ == "char":
            self.gen.emit("    xor ah, ah")
            self.gen.emit("    mov al, [bx]")
        else:
            self.gen.emit("    mov ax, [bx]")
        self.last_primary_type = "int" if member.is_bitfield else member.typ

    def _emit_store_member_at_bx(self, member: StructMember) -> None:
        """Store AX into member at BX. Bit-fields leave the masked value in AX."""
        if member.is_bitfield:
            self.gen.emit(f"    and ax, {member.value_mask}")
            self.gen.emit("    push ax")
            if member.bit_offset:
                self.gen.emit(f"    shl ax, {member.bit_offset}")
            self.gen.emit("    mov cx, ax")
            self.gen.emit("    mov ax, [bx]")
            clear = (~member.bit_mask) & 0xFFFF
            self.gen.emit(f"    and ax, {clear}")
            self.gen.emit("    or ax, cx")
            self.gen.emit("    mov [bx], ax")
            self.gen.emit("    pop ax")
        elif member.typ == "char":
            self.gen.emit("    mov [bx], al")
        else:
            self.gen.emit("    mov [bx], ax")

    def _emit_load_member(self, base_name: str, member: StructMember) -> None:
        self._emit_member_addr_bx(base_name, member)
        self._emit_load_member_at_bx(member)

    def _emit_store_member(self, base_name: str, member: StructMember) -> None:
        """Store AX into named object's member; leave assigned (masked) value in AX for bit-fields."""
        if member.is_bitfield:
            self.gen.emit(f"    and ax, {member.value_mask}")
            self.gen.emit("    push ax")
            if member.bit_offset:
                self.gen.emit(f"    shl ax, {member.bit_offset}")
            self.gen.emit("    mov cx, ax")
            self._emit_member_addr_bx(base_name, member)
            self.gen.emit("    mov ax, [bx]")
            clear = (~member.bit_mask) & 0xFFFF
            self.gen.emit(f"    and ax, {clear}")
            self.gen.emit("    or ax, cx")
            self.gen.emit("    mov [bx], ax")
            self.gen.emit("    pop ax")
        else:
            self._emit_member_addr_bx(base_name, member)
            self._emit_store_member_at_bx(member)

    def _emit_lea(self, reg: str, name: str) -> None:
        """Load address of named object into reg (si/di/ax/bx)."""
        _typ, off = self._lookup(name)
        if self._is_local(name):
            self.gen.emit(f"    lea {reg}, [bp{off:+d}]")
        else:
            self.gen.emit(f"    lea {reg}, [{self._label_for(name)}]")

    def _emit_struct_copy(self, dest_name: str, src_name: str, line: int, col: int) -> None:
        """Copy a struct object src_name into dest_name (same type)."""
        dt, _ = self._lookup(dest_name)
        if dt is None:
            raise CompileError(f"undefined identifier '{dest_name}'", line, col)
        styp, _ = self._lookup(src_name)
        if styp is None:
            raise CompileError(f"undefined identifier '{src_name}'", line, col)
        if not self._is_struct_type(dt) or not self._is_struct_type(styp):
            raise CompileError("struct assignment requires struct objects", line, col)
        if dt != styp:
            raise CompileError(
                f"struct assignment type mismatch ('{dt}' vs '{styp}')", line, col
            )
        if dest_name == src_name:
            return
        st = self._require_complete_struct(dt, line, col)
        nbytes = st.size
        if nbytes <= 0:
            return
        self._emit_lea("si", src_name)
        self._emit_lea("di", dest_name)
        self.gen.emit("    push ds")
        self.gen.emit("    pop es")
        self.gen.emit(f"    mov cx, {nbytes}")
        self.gen.emit("    cld")
        self.gen.emit("    rep movsb")

    def _parse_struct_copy_from_rhs(self, dest_name: str) -> None:
        """Consume RHS of struct assignment ('=' already eaten) and copy into dest."""
        if not self._at(T_IDENT):
            raise CompileError(
                "struct assignment requires a struct object",
                self.cur_token.line,
                self.cur_token.col,
            )
        src_tok = self.cur_token
        src = src_tok.value
        self._advance()
        if self._at(T_ASSIGN):
            self._advance()
            self._parse_struct_copy_from_rhs(src)
            self._emit_struct_copy(dest_name, src, src_tok.line, src_tok.col)
            return
        if self._at(T_LBRACKET) or self._at(T_DOT) or self._at(T_ARROW):
            raise CompileError(
                "struct assignment requires a struct object",
                self.cur_token.line,
                self.cur_token.col,
            )
        self._emit_struct_copy(dest_name, src, src_tok.line, src_tok.col)

    def _alloc_local(self, name: str, t: str, size: int, line: int, col: int) -> None:
        """Reserve stack space for a local and record it in self.locals."""
        if size > 0:
            if self._is_struct_type(t):
                st = self._require_complete_struct(t, line, col)
                self._align_local(2)
                nbytes = size * st.size
            else:
                elem = self._type_size(t)
                if elem <= 0:
                    raise CompileError(f"cannot declare array of '{t}'", line, col)
                if elem >= 2:
                    self._align_local(2)
                nbytes = size * elem
            if nbytes > 0:
                self.gen.emit(f"    sub sp, {nbytes}")
                self.local_offset += nbytes
            self.locals[name] = (t, -self.local_offset)
            self.arrays.add(name)
            self.array_lengths[name] = size
        elif self._is_struct_type(t):
            st = self._require_complete_struct(t, line, col)
            self._align_local(2)
            sz = st.size
            if sz > 0:
                self.gen.emit(f"    sub sp, {sz}")
                self.local_offset += sz
            self.locals[name] = (t, -self.local_offset)
        elif t.endswith("*") or t == "int" or self._is_enum_type(t):
            if self._is_struct_ptr(t):
                self._require_complete_struct(self._struct_obj_type(t), line, col)
            self.gen.emit("    sub sp, 2")
            self.local_offset += 2
            self.locals[name] = (t, -self.local_offset)
        else:
            self._reject_void_object(t, line, col)
            sz = 1
            self.gen.emit(f"    sub sp, {sz}")
            self.local_offset += sz
            self.locals[name] = (t, -self.local_offset)

    def _store_local_scalar(self, name: str) -> None:
        """Store AX into a scalar/pointer local (or global static alias)."""
        typ, off = self._lookup(name)
        if self._is_local(name):
            if typ == "char":
                self.gen.emit(f"    mov [bp{off:+d}], al")
            else:
                self.gen.emit(f"    mov [bp{off:+d}], ax")
        else:
            self.gen.emit(f"    mov [{self._label_for(name)}], ax")

    def _zero_local_bytes(self, off: int, nbytes: int) -> None:
        if nbytes <= 0:
            return
        self.gen.emit(f"    lea di, [bp{off:+d}]")
        self.gen.emit("    push ds")
        self.gen.emit("    pop es")
        self.gen.emit(f"    mov cx, {nbytes}")
        self.gen.emit("    xor al, al")
        self.gen.emit("    cld")
        self.gen.emit("    rep stosb")

    def _emit_local_initializer(self, name: str, t: str, size: int) -> None:
        """Consume '= ...' after a local has been allocated."""
        self._expect(T_ASSIGN)
        if size > 0:
            _, off = self.locals[name]
            elem = self._type_size(t)
            nbytes = size * elem
            self._zero_local_bytes(off, nbytes)
            if self._at(T_STRING):
                if t != "char":
                    raise CompileError(
                        "string initializer only for char array",
                        self.cur_token.line,
                        self.cur_token.col,
                    )
                s = self.cur_token.value
                self._advance()
                data = list(s.encode("latin-1"))
                if len(data) + 1 <= size:
                    data = data + [0]
                else:
                    data = data[: size - 1] + [0]
                for i, b in enumerate(data[:size]):
                    self.gen.emit(f"    mov al, {b}")
                    self.gen.emit(f"    mov [bp{off + i:+d}], al")
            elif self._at(T_LBRACE):
                self._advance()
                idx = 0
                while not self._at(T_RBRACE) and not self._at(T_EOF):
                    if idx >= size:
                        raise CompileError(
                            f"too many initializers (array size {size})",
                            self.cur_token.line,
                            self.cur_token.col,
                        )
                    self._expr_assign()
                    elem_off = off + idx * elem
                    if t == "char":
                        self.gen.emit(f"    mov [bp{elem_off:+d}], al")
                    else:
                        self.gen.emit(f"    mov [bp{elem_off:+d}], ax")
                    idx += 1
                    if not self._at(T_RBRACE):
                        self._expect(T_COMMA)
                self._expect(T_RBRACE)
            else:
                raise CompileError(
                    "array initializer must be string or { values }",
                    self.cur_token.line,
                    self.cur_token.col,
                )
            return
        if self._is_struct_type(t):
            if self._at(T_LBRACE):
                raise CompileError(
                    "struct initializers are not supported",
                    self.cur_token.line,
                    self.cur_token.col,
                )
            self._parse_struct_copy_from_rhs(name)
            return
        if self._at(T_LBRACE):
            self._advance()
            self._expr_assign()
            self._expect(T_RBRACE)
        else:
            self._expr_assign()
        self._store_local_scalar(name)

    def _lookup_member(self, struct_typ: str, member_name: str, line: int, col: int) -> StructMember:
        st = self._require_complete_struct(struct_typ, line, col)
        m = st.members.get(member_name)
        if m is None:
            raise CompileError(
                f"struct '{st.tag}' has no member '{member_name}'", line, col
            )
        return m

    def _parse_sizeof_type(self) -> str:
        """Parse a type name inside sizeof(...), without allowing a new struct/enum definition."""
        if self._at(T_STRUCT):
            self._advance()
            if not self._at(T_IDENT):
                raise CompileError("expected struct tag", self.cur_token.line, self.cur_token.col)
            tag = self.cur_token.value
            self._advance()
            if self._at(T_LBRACE):
                raise CompileError(
                    "struct definition not allowed in sizeof",
                    self.cur_token.line,
                    self.cur_token.col,
                )
            typ = f"struct {tag}"
        elif self._at(T_ENUM):
            self._advance()
            if not self._at(T_IDENT):
                raise CompileError("expected enum tag", self.cur_token.line, self.cur_token.col)
            tag = self.cur_token.value
            self._advance()
            if self._at(T_LBRACE):
                raise CompileError(
                    "enum definition not allowed in sizeof",
                    self.cur_token.line,
                    self.cur_token.col,
                )
            typ = f"enum {tag}"
        else:
            typ = self._type_spec()
            if typ is None:
                raise CompileError("expected type name", self.cur_token.line, self.cur_token.col)
        if self._at(T_STAR):
            self._advance()
            typ = typ + "*"
        return typ

    def _emit_cast(self, typ: str) -> None:
        """Apply a cast to the value in AX and update last_primary_type."""
        if typ == "void":
            raise CompileError("cast to void is not supported", self.cur_token.line, self.cur_token.col)
        if typ == "char":
            self.gen.emit("    and ax, 255")
            self.last_primary_type = "char"
        elif typ.startswith("enum "):
            self.last_primary_type = "int"
        else:
            self.last_primary_type = typ

    def _sizeof_value(self, typ: str, line: int, col: int) -> int:
        if typ == "void":
            raise CompileError("sizeof(void) is not supported", line, col)
        if self._is_struct_type(typ):
            self._require_complete_struct(typ, line, col)
        sz = self._type_size(typ)
        if sz <= 0:
            raise CompileError(f"cannot take sizeof('{typ}')", line, col)
        return sz

    def _sizeof_expr_no_eval(self) -> int:
        """Compute sizeof an lvalue expression without emitting code."""
        if not self._at(T_IDENT):
            raise CompileError(
                "sizeof(expr) supports identifiers and member access only",
                self.cur_token.line,
                self.cur_token.col,
            )
        name_tok = self.cur_token
        name = name_tok.value
        self._advance()
        typ, _off = self._lookup(name)
        if typ is None:
            raise CompileError(f"undefined identifier '{name}'", name_tok.line, name_tok.col)
        if self._at(T_DOT) or self._at(T_ARROW):
            is_arrow = self._at(T_ARROW)
            self._advance()
            if not self._at(T_IDENT):
                raise CompileError("expected member name", self.cur_token.line, self.cur_token.col)
            mname_tok = self.cur_token
            mname = mname_tok.value
            self._advance()
            if is_arrow:
                if not self._is_struct_ptr(typ):
                    raise CompileError(
                        f"arrow member access on non-struct-pointer '{name}'",
                        name_tok.line,
                        name_tok.col,
                    )
                base = self._struct_obj_type(typ)
            else:
                if not self._is_struct_type(typ):
                    raise CompileError(
                        f"dot member access on non-struct '{name}'",
                        name_tok.line,
                        name_tok.col,
                    )
                base = typ
            member = self._lookup_member(base, mname, mname_tok.line, mname_tok.col)
            if member.is_bitfield:
                raise CompileError(
                    "cannot apply sizeof to a bit-field", mname_tok.line, mname_tok.col
                )
            return 2 if member.typ == "int" else 1
        if name in self.array_lengths:
            return self.array_lengths[name] * self._sizeof_value(typ, name_tok.line, name_tok.col)
        return self._sizeof_value(typ, name_tok.line, name_tok.col)
    def _emit_global_storage(
        self,
        name: str,
        typ: str,
        size: int,
        is_extern: bool,
        has_init: bool,
        is_static: bool = False,
    ) -> None:
        """Emit data/bss/extern for a global. Caller already consumed '=' if has_init."""
        if is_extern:
            self.gen.emit(f".extern {name}")
            self._expect(T_SEMI)
            return
        if has_init:
            if self._is_struct_type(typ):
                raise CompileError(
                    "struct initializers are not supported",
                    self.cur_token.line,
                    self.cur_token.col,
                )
            # Unique data section so ld --gc-sections can drop unused globals.
            self.gen.emit(f".section .data.{name}")
            if not is_static:
                self.gen.emit(f".global {name}")
            self.gen.emit(f"{name}:")
            if size > 0:
                elem_size = 2 if typ == "int" else 1
                if self._at(T_LBRACE):
                    self._advance()
                    values = []
                    while not self._at(T_RBRACE) and not self._at(T_EOF):
                        if not self._at(T_NUMBER):
                            raise CompileError("brace initializer element must be a number", self.cur_token.line, self.cur_token.col)
                        values.append(self.cur_token.value & (0xFFFF if typ == "int" else 0xFF))
                        self._advance()
                        if not self._at(T_RBRACE):
                            self._expect(T_COMMA)
                    self._expect(T_RBRACE)
                    if not values or (len(values) == 1 and values[0] == 0):
                        self.gen.emit(f"    .space {size * elem_size}, 0")
                    else:
                        if len(values) > size:
                            raise CompileError(f"too many initializers (have {len(values)}, array size {size})", self.cur_token.line, self.cur_token.col)
                        if typ == "int":
                            self.gen.emit("    .word " + ", ".join(str(v) for v in values))
                        else:
                            self.gen.emit("    .byte " + ", ".join(str(v) for v in values))
                        if len(values) < size:
                            self.gen.emit(f"    .space {(size - len(values)) * elem_size}, 0")
                elif self._at(T_STRING):
                    if typ != "char":
                        raise CompileError("string initializer only for char array", self.cur_token.line, self.cur_token.col)
                    s = self.cur_token.value
                    self._advance()
                    bytes_val = s.encode("latin-1")
                    if len(bytes_val) + 1 <= size:
                        bytes_list = list(bytes_val) + [0] + [0] * (size - len(bytes_val) - 1)
                    else:
                        bytes_list = list(bytes_val[: size - 1]) + [0]
                    if bytes_list:
                        self.gen.emit("    .byte " + ", ".join(str(b) for b in bytes_list))
                else:
                    raise CompileError("array initializer must be string or { numbers }", self.cur_token.line, self.cur_token.col)
            else:
                # scalar / pointer: int x = 5; or char c = 'a'; or T *p = 0;
                if typ.endswith("*") or typ == "int" or self._is_enum_type(typ):
                    if not self._at(T_NUMBER):
                        raise CompileError(
                            "int/pointer initializer must be a number",
                            self.cur_token.line,
                            self.cur_token.col,
                        )
                    val = self.cur_token.value
                    self._advance()
                    self.gen.emit(f"    .word {val & 0xFFFF}")
                else:
                    if self._at(T_NUMBER):
                        val = self.cur_token.value
                        self._advance()
                        self.gen.emit(f"    .byte {val & 0xFF}")
                    elif self._at(T_STRING):
                        s = self.cur_token.value
                        self._advance()
                        if len(s) < 1:
                            raise CompileError("char initializer string must have one character", self.cur_token.line, self.cur_token.col)
                        self.gen.emit(f"    .byte {ord(s[0]) & 0xFF}")
                    else:
                        raise CompileError("char initializer must be number or character literal", self.cur_token.line, self.cur_token.col)
        else:
            if size > 0:
                if self._is_struct_type(typ):
                    self._require_complete_struct(typ, self.cur_token.line, self.cur_token.col)
                    nbytes = size * self._type_size(typ)
                else:
                    nbytes = size * (2 if typ == "int" else 1)
            else:
                nbytes = self._type_size(typ)
                if nbytes == 0 and self._is_struct_type(typ):
                    self._require_complete_struct(typ, self.cur_token.line, self.cur_token.col)
                    nbytes = self._type_size(typ)
            if self.exe_entry:
                self.gen.emit(f".section .bss.{name}")
                if not is_static:
                    self.gen.emit(f".global {name}")
                self.gen.emit(f"{name}:")
                self.gen.emit(f"    .space {nbytes}")
            else:
                self.gen.emit(f".section .data.{name}")
                if not is_static:
                    self.gen.emit(f".global {name}")
                self.gen.emit(f"{name}:")
                if size > 0 or self._is_struct_type(typ):
                    self.gen.emit(f"    .space {nbytes}, 0")
                elif typ.endswith("*") or typ == "int" or self._is_enum_type(typ):
                    self.gen.emit("    .word 0")
                else:
                    self.gen.emit("    .byte 0")
        self._expect(T_SEMI)
        self.gen.emit(".section .text")

    def compile(self) -> str:
        self._advance()
        self.gen.emit(".code16")
        self.gen.emit(".intel_syntax noprefix")
        self.gen.emit(".section .text")

        if self.com_entry:
            # DOS .COM: linked at 0x100, CS=DS=ES=PSP segment.
            self.gen.emit(".section .text._start")
            self.gen.emit(".global _start")
            self.gen.emit("_start:")
            self.gen.emit("    push cs")
            self.gen.emit("    pop ds")
            self.gen.emit("    push cs")
            self.gen.emit("    pop es")
            self.gen.emit("    call main")
            self.gen.emit("    mov ah, 0x4C")
            self.gen.emit("    int 0x21")
        elif self.overlay_entry:
            # DOS overlay body (INT 21h/4B03): tiny model, return via retf.
            self.gen.emit(".section .text._start")
            self.gen.emit(".global _start")
            self.gen.emit("_start:")
            self.gen.emit("    push cs")
            self.gen.emit("    pop ds")
            self.gen.emit("    push cs")
            self.gen.emit("    pop es")
            self.gen.emit("    call main")
            self.gen.emit("    retf")
        elif self.exe_entry:
            # Small-model MZ: CS=code, SS=DS=data (set by loader SS); zero BSS.
            self.gen.emit(".section .text._start")
            self.gen.emit(".global _start")
            self.gen.emit("_start:")
            self.gen.emit("    mov ax, ss")
            self.gen.emit("    mov ds, ax")
            self.gen.emit("    mov es, ax")
            self.gen.emit("    mov di, offset __bss_start")
            self.gen.emit("    mov cx, offset __bss_end")
            self.gen.emit("    sub cx, di")
            self.gen.emit("    xor al, al")
            self.gen.emit("    cld")
            self.gen.emit("    rep stosb")
            self.gen.emit("    call main")
            self.gen.emit("    mov ah, 0x4C")
            self.gen.emit("    int 0x21")
        elif self.module_name:
            self.gen.emit(".section .text.module_header")
            self.gen.emit(".global module_header")
            self.gen.emit("module_header:")
            self.gen.emit('    .ascii "MOD0"')
            self.gen.emit("    .word module_entry")
            self.gen.emit(f'    .ascii "{self.module_name}"')
            self.gen.emit("    .byte 0x00")
            self.gen.emit(".section .text.module_entry")
            self.gen.emit("module_entry:")
            self.gen.emit("    push ax")
            self.gen.emit("    push bx")
            self.gen.emit("    push cx")
            self.gen.emit("    push dx")
            self.gen.emit("    push si")
            self.gen.emit("    push di")
            self.gen.emit("    push ds")
            self.gen.emit("    push es")
            self.gen.emit("    push cs")
            self.gen.emit("    pop ds")
            self.gen.emit("    push cs")
            self.gen.emit("    pop es")
            self.gen.emit("    call main")
            self.gen.emit("    pop es")
            self.gen.emit("    pop ds")
            self.gen.emit("    pop di")
            self.gen.emit("    pop si")
            self.gen.emit("    pop dx")
            self.gen.emit("    pop cx")
            self.gen.emit("    pop bx")
            self.gen.emit("    pop ax")
            self.gen.emit("    retf")

        while not self._at(T_EOF):
            # Optional storage class (mutually exclusive)
            is_extern = False
            is_static = False
            if self._at(T_EXTERN):
                is_extern = True
                self._advance()
                if self._at(T_STATIC):
                    raise CompileError(
                        "cannot combine extern and static",
                        self.cur_token.line,
                        self.cur_token.col,
                    )
            elif self._at(T_STATIC):
                is_static = True
                self._advance()
                if self._at(T_EXTERN):
                    raise CompileError(
                        "cannot combine static and extern",
                        self.cur_token.line,
                        self.cur_token.col,
                    )
            t = self._parse_decl_type()
            if t is None:
                break
            # struct Tag { ... };  or enum Tag { ... }; / enum { ... };
            if (self._is_struct_type(t) or self._is_enum_type(t) or t == "enum") and self._at(T_SEMI):
                self._advance()
                continue
            if t == "enum":
                raise CompileError(
                    "anonymous enum cannot declare a variable (give it a tag)",
                    self.cur_token.line,
                    self.cur_token.col,
                )
            t = self._apply_pointer_star(t)
            if not self._at(T_IDENT):
                raise CompileError("expected identifier", self.cur_token.line, self.cur_token.col)
            next_t = self._peek_next()
            if next_t.kind != T_LPAREN:
                # Global variable (scalar, struct, pointer, or array), optional initializer
                self._reject_void_object(t, self.cur_token.line, self.cur_token.col)
                name = self.cur_token.value
                self._advance()
                size = 0
                is_array = False
                if self._at(T_LBRACKET):
                    if self._is_struct_ptr(t):
                        raise CompileError(
                            "arrays of struct pointers are not supported",
                            self.cur_token.line,
                            self.cur_token.col,
                        )
                    self._advance()
                    is_array = True
                    if self._at(T_NUMBER):
                        size = self.cur_token.value
                        self._advance()
                    elif not is_extern:
                        raise CompileError(
                            "array size required (use extern for incomplete [])",
                            self.cur_token.line,
                            self.cur_token.col,
                        )
                    if is_array and size == 0:
                        size = 1
                    self._expect(T_RBRACKET)
                if self._is_struct_type(t) and not is_extern:
                    self._require_complete_struct(t, self.cur_token.line, self.cur_token.col)
                if self._is_struct_ptr(t) and not is_extern:
                    # Pointee must be complete for -> / sizeof(*p) clarity.
                    self._require_complete_struct(
                        self._struct_obj_type(t), self.cur_token.line, self.cur_token.col
                    )
                has_init = self._at(T_ASSIGN)
                if has_init:
                    self._advance()  # consume =
                if is_extern and has_init:
                    raise CompileError(
                        "extern declaration cannot have an initializer",
                        self.cur_token.line,
                        self.cur_token.col,
                    )
                self.globals[name] = (t, size)
                if size > 0:
                    self.arrays.add(name)
                    self.array_lengths[name] = size
                self._emit_global_storage(name, t, size, is_extern, has_init, is_static)
                continue

            # Function
            if self._is_struct_type(t):
                raise CompileError(
                    "struct return types are not supported",
                    self.cur_token.line,
                    self.cur_token.col,
                )
            name = self._expect(T_IDENT).value
            self._expect(T_LPAREN)
            params: list[tuple[str, str]] = []
            while not self._at(T_RPAREN):
                pt = self._parse_decl_type()
                if pt is None:
                    break
                if pt == "void" and self._at(T_RPAREN):
                    break
                pt = self._apply_pointer_star(pt)
                self._reject_void_object(pt, self.cur_token.line, self.cur_token.col)
                if self._is_struct_type(pt):
                    raise CompileError(
                        "struct parameters are not supported (use struct pointers)",
                        self.cur_token.line,
                        self.cur_token.col,
                    )
                if self._is_struct_ptr(pt):
                    self._require_complete_struct(
                        self._struct_obj_type(pt), self.cur_token.line, self.cur_token.col
                    )
                pname = self._expect(T_IDENT).value
                params.append((pname, pt))
                if not self._at(T_RPAREN):
                    self._expect(T_COMMA)
            self._expect(T_RPAREN)
            if self._at(T_SEMI):
                self._advance()
                continue
            self._expect(T_LBRACE)

            self.params = params
            self.locals = {}
            self.static_locals = {}
            self.local_offset = 0
            self.current_func = name
            # Drop previous function's local arrays; keep global arrays.
            self.arrays = {n for n, (_t, sz) in self.globals.items() if sz > 0}
            self.array_lengths = {n: sz for n, (_t, sz) in self.globals.items() if sz > 0}
            # Args pushed left-to-right: last arg at [bp+4], first at [bp+4+2*(n-1)]
            for i, (pname, ptyp) in enumerate(params):
                self.locals[pname] = (ptyp, 4 + 2 * (len(params) - 1 - i))

            # Emit function prologue, then body, then epilogue.
            # Per-function text section enables ld --gc-sections.
            self.gen.emit(f".section .text.{name}")
            if not is_static:
                self.gen.emit(f".global {name}")
            self.gen.emit(f"{name}:")
            self.gen.emit("    push bp")
            self.gen.emit("    mov bp, sp")
            self._parse_statements()
            self._expect(T_RBRACE)
            self.gen.emit("    mov sp, bp")
            self.gen.emit("    pop bp")
            self.gen.emit("    ret")
            self.current_func = None
        # Emit .rodata for string literals
        if self.gen.strings:
            self.gen.emit(".section .rodata")
            for label, content in self.gen.strings:
                escaped = content.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n").replace("\r", "\\r")
                self.gen.emit(f'{label}: .asciz "{escaped}"')

        return "\n".join(self.gen.lines) + "\n"

    def _parse_statements(self):
        while not self._at(T_RBRACE) and not self._at(T_EOF):
            self._parse_statement()

    def _parse_statement(self):
        if self._at(T_SEMI):
            self._advance()
            return
        if self._at(T_LBRACE):
            self._advance()
            while not self._at(T_RBRACE) and not self._at(T_EOF):
                self._parse_statement()
            self._expect(T_RBRACE)
            return
        is_static_local = False
        if self._at(T_STATIC):
            is_static_local = True
            self._advance()
        t = self._parse_decl_type()
        if t is not None:
            if (
                self._is_struct_type(t) or self._is_enum_type(t) or t == "enum"
            ) and self._at(T_SEMI):
                # Local struct/enum definition only
                self._advance()
                return
            if t == "enum":
                raise CompileError(
                    "anonymous enum cannot declare a variable (give it a tag)",
                    self.cur_token.line,
                    self.cur_token.col,
                )
            t = self._apply_pointer_star(t)
            self._reject_void_object(t, self.cur_token.line, self.cur_token.col)
            while True:
                name_tok = self.cur_token
                name = self._expect(T_IDENT).value
                size = 0
                if self._at(T_LBRACKET):
                    if self._is_struct_ptr(t):
                        raise CompileError(
                            "arrays of struct pointers are not supported",
                            self.cur_token.line,
                            self.cur_token.col,
                        )
                    self._advance()
                    size = self._expect(T_NUMBER).value
                    self._expect(T_RBRACKET)

                if is_static_local:
                    if name in self.locals or name in self.static_locals:
                        raise CompileError(
                            f"redefinition of '{name}'",
                            self.cur_token.line,
                            self.cur_token.col,
                        )
                    if self.current_func is None:
                        raise CompileError(
                            "static local outside function",
                            self.cur_token.line,
                            self.cur_token.col,
                        )
                    if self._is_struct_type(t) and size == 0:
                        self._require_complete_struct(
                            t, self.cur_token.line, self.cur_token.col
                        )
                    if self._is_struct_ptr(t):
                        self._require_complete_struct(
                            self._struct_obj_type(t),
                            self.cur_token.line,
                            self.cur_token.col,
                        )
                    self._static_local_seq += 1
                    label = f"{self.current_func}__static_{name}_{self._static_local_seq}"
                    has_init = self._at(T_ASSIGN)
                    if has_init:
                        self._advance()
                    if self._at(T_COMMA):
                        raise CompileError(
                            "comma-separated static locals are not supported",
                            self.cur_token.line,
                            self.cur_token.col,
                        )
                    self.globals[label] = (t, size)
                    self.static_locals[name] = label
                    if size > 0:
                        self.arrays.add(name)
                        self.array_lengths[name] = size
                    # Emit as file-scope static data (no .global); stay in function text after.
                    self._emit_global_storage(
                        label, t, size, False, has_init, is_static=True
                    )
                    self.gen.emit(f".section .text.{self.current_func}")
                    return

                if name in self.locals or name in self.static_locals:
                    raise CompileError(
                        f"redefinition of '{name}'",
                        name_tok.line,
                        name_tok.col,
                    )
                self._alloc_local(name, t, size, name_tok.line, name_tok.col)
                if self._at(T_ASSIGN):
                    self._emit_local_initializer(name, t, size)
                if not self._at(T_COMMA):
                    break
                self._advance()
            self._expect(T_SEMI)
            return
        if is_static_local:
            raise CompileError(
                "expected declaration after static",
                self.cur_token.line,
                self.cur_token.col,
            )
        if self._at(T_IF):
            self._advance()
            self._expect(T_LPAREN)
            self._expr()
            self._expect(T_RPAREN)
            lfalse = self.gen.new_label()
            self.gen.emit("    cmp ax, 0")
            self.gen.emit(f"    je {lfalse}")
            self._parse_statement()
            if self._at(T_ELSE):
                self._advance()
                lend = self.gen.new_label()
                self.gen.emit(f"    jmp {lend}")
                self.gen.emit(f"{lfalse}:")
                self._parse_statement()
                self.gen.emit(f"{lend}:")
            else:
                self.gen.emit(f"{lfalse}:")
            return
        if self._at(T_DO):
            self._advance()
            lstart = self.gen.new_label()
            lcont = self.gen.new_label()
            lend = self.gen.new_label()
            self.break_labels.append(lend)
            self.continue_labels.append(lcont)
            self.gen.emit(f"{lstart}:")
            self._parse_statement()
            self.gen.emit(f"{lcont}:")
            self._expect(T_WHILE)
            self._expect(T_LPAREN)
            self._expr()
            self._expect(T_RPAREN)
            self._expect(T_SEMI)
            self.gen.emit("    cmp ax, 0")
            self.gen.emit(f"    jne {lstart}")
            self.gen.emit(f"{lend}:")
            self.break_labels.pop()
            self.continue_labels.pop()
            return
        if self._at(T_WHILE):
            self._advance()
            self._expect(T_LPAREN)
            lstart = self.gen.new_label()
            lend = self.gen.new_label()
            self.break_labels.append(lend)
            self.continue_labels.append(lstart)
            self.gen.emit(f"{lstart}:")
            self._expr()
            self._expect(T_RPAREN)
            self.gen.emit("    cmp ax, 0")
            self.gen.emit(f"    je {lend}")
            self._parse_statement()
            self.gen.emit(f"    jmp {lstart}")
            self.gen.emit(f"{lend}:")
            self.break_labels.pop()
            self.continue_labels.pop()
            return
        if self._at(T_SWITCH):
            self._advance()
            self._expect(T_LPAREN)
            self._expr()
            self._expect(T_RPAREN)
            self.gen.emit("    push ax")
            ldisp = self.gen.new_label()
            lend = self.gen.new_label()
            self.gen.emit(f"    jmp {ldisp}")
            self.break_labels.append(lend)
            self._expect(T_LBRACE)
            cases: list[tuple[int, str]] = []
            default_lab: str | None = None
            while not self._at(T_RBRACE) and not self._at(T_EOF):
                if self._at(T_CASE):
                    while self._at(T_CASE):
                        self._advance()
                        neg = False
                        if self._at(T_MINUS):
                            self._advance()
                            neg = True
                        if self._at(T_NUMBER):
                            val = int(self.cur_token.value)
                            self._advance()
                        elif (
                            not neg
                            and self._at(T_IDENT)
                            and self.cur_token.value in self.enumerators
                        ):
                            val = self.enumerators[self.cur_token.value]
                            self._advance()
                        else:
                            raise CompileError(
                                "case label must be an integer constant",
                                self.cur_token.line,
                                self.cur_token.col,
                            )
                        if neg:
                            val = -val
                        self._expect(T_COLON)
                        lab = self.gen.new_label()
                        cases.append((val, lab))
                        self.gen.emit(f"{lab}:")
                    while (
                        not self._at(T_CASE)
                        and not self._at(T_DEFAULT)
                        and not self._at(T_RBRACE)
                        and not self._at(T_EOF)
                    ):
                        self._parse_statement()
                elif self._at(T_DEFAULT):
                    if default_lab is not None:
                        raise CompileError(
                            "duplicate default in switch",
                            self.cur_token.line,
                            self.cur_token.col,
                        )
                    self._advance()
                    self._expect(T_COLON)
                    default_lab = self.gen.new_label()
                    self.gen.emit(f"{default_lab}:")
                    while (
                        not self._at(T_CASE)
                        and not self._at(T_DEFAULT)
                        and not self._at(T_RBRACE)
                        and not self._at(T_EOF)
                    ):
                        self._parse_statement()
                else:
                    raise CompileError(
                        "expected case or default in switch",
                        self.cur_token.line,
                        self.cur_token.col,
                    )
            self._expect(T_RBRACE)
            # Bodies are above; do not fall into the compare chain.
            self.gen.emit(f"    jmp {lend}")
            self.gen.emit(f"{ldisp}:")
            self.gen.emit("    pop ax")
            for val, lab in cases:
                self.gen.emit(f"    cmp ax, {val & 0xFFFF}")
                self.gen.emit(f"    je {lab}")
            if default_lab is not None:
                self.gen.emit(f"    jmp {default_lab}")
            else:
                self.gen.emit(f"    jmp {lend}")
            self.gen.emit(f"{lend}:")
            self.break_labels.pop()
            return
        if self._at(T_FOR):
            self._advance()
            self._expect(T_LPAREN)
            # for (init; cond; incr) body
            # Emit order must be: init, cond-check, body, incr, jmp cond.
            # continue jumps to the incr label (before increment runs).
            if not self._at(T_SEMI):
                self._expr()
            self._expect(T_SEMI)
            lcond = self.gen.new_label()
            lcont = self.gen.new_label()
            lend = self.gen.new_label()
            self.break_labels.append(lend)
            self.continue_labels.append(lcont)
            self.gen.emit(f"{lcond}:")
            if not self._at(T_SEMI):
                self._expr()
                self.gen.emit("    cmp ax, 0")
                self.gen.emit(f"    je {lend}")
            self._expect(T_SEMI)
            # Capture increment tokens without emitting; replay after the body.
            # Sentinel EOF prevents the replayed expr from consuming tokens that
            # already follow the loop (the body has advanced the source lexer).
            incr_tokens: list[Token] = []
            depth = 0
            while not self._at(T_EOF):
                if self._at(T_RPAREN) and depth == 0:
                    break
                if self._at(T_LPAREN) or self._at(T_LBRACKET):
                    depth += 1
                elif self._at(T_RPAREN) or self._at(T_RBRACKET):
                    depth -= 1
                incr_tokens.append(self.cur_token)
                self._advance()
            self._expect(T_RPAREN)
            self._parse_statement()
            self.gen.emit(f"{lcont}:")
            if incr_tokens:
                saved = self.cur_token
                eof = Token(T_EOF, None, saved.line, saved.col)
                self.lexer._pending = list(incr_tokens) + [eof] + self.lexer._pending
                self._advance()
                self._expr()
                self.cur_token = saved
                while self.lexer._pending and self.lexer._pending[0].kind == T_EOF:
                    self.lexer._pending.pop(0)
            self.gen.emit(f"    jmp {lcond}")
            self.gen.emit(f"{lend}:")
            self.break_labels.pop()
            self.continue_labels.pop()
            return
        if self._at(T_RETURN):
            self._advance()
            if not self._at(T_SEMI):
                self._expr()
            self.gen.emit("    mov sp, bp")
            self.gen.emit("    pop bp")
            self.gen.emit("    ret")
            self._expect(T_SEMI)
            return
        if self._at(T_BREAK):
            self._advance()
            if not self.break_labels:
                raise CompileError("break outside loop", self.cur_token.line, self.cur_token.col)
            self.gen.emit(f"    jmp {self.break_labels[-1]}")
            self._expect(T_SEMI)
            return
        if self._at(T_CONTINUE):
            self._advance()
            if not self.continue_labels:
                raise CompileError("continue outside loop", self.cur_token.line, self.cur_token.col)
            self.gen.emit(f"    jmp {self.continue_labels[-1]}")
            self._expect(T_SEMI)
            return
        if self._at(T_ASM):
            self._advance()
            self._expect(T_LPAREN)
            s = self._expect(T_STRING).value
            self._expect(T_RPAREN)
            self._expect(T_SEMI)
            self.gen.emit(f"    {s}")
            return
        # Expression statement
        self._expr()
        self._expect(T_SEMI)

    def _expr(self):
        self._expr_assign()

    def _expr_assign(self):
        if self._at(T_IDENT):
            t2 = self._peek_next()
            if t2.kind == T_ASSIGN:
                name = self.cur_token.value
                self._advance()
                self._advance()
                typ, off = self._lookup(name)
                if typ is None:
                    raise CompileError(f"undefined identifier '{name}'", self.cur_token.line, self.cur_token.col)
                if self._is_struct_type(typ):
                    self._parse_struct_copy_from_rhs(name)
                    return
                self._expr_assign()
                if self._is_local(name):
                    if typ == "char":
                        self.gen.emit(f"    mov [bp{off:+d}], al")
                    else:
                        self.gen.emit(f"    mov [bp{off:+d}], ax")
                else:
                    self.gen.emit(f"    mov [{self._label_for(name)}], ax")
                return
            if t2.kind == T_DOT:
                name = self.cur_token.value
                name_tok = self.cur_token
                self._advance()
                self._expect(T_DOT)
                if not self._at(T_IDENT):
                    raise CompileError(
                        "expected member name", self.cur_token.line, self.cur_token.col
                    )
                mname_tok = self.cur_token
                mname = mname_tok.value
                self._advance()
                typ, _off = self._lookup(name)
                if typ is None:
                    raise CompileError(
                        f"undefined identifier '{name}'", name_tok.line, name_tok.col
                    )
                if not self._is_struct_type(typ):
                    raise CompileError(
                        f"dot member access on non-struct '{name}'",
                        name_tok.line,
                        name_tok.col,
                    )
                member = self._lookup_member(typ, mname, mname_tok.line, mname_tok.col)
                if self._at(T_ASSIGN):
                    self._advance()
                    self._expr_assign()
                    self._emit_store_member(name, member)
                    return
                # Load member then continue expression (e.g. s.a + 1)
                self._emit_load_member(name, member)
                self._expr_lor_continue()
                return
            if t2.kind == T_ARROW:
                name = self.cur_token.value
                name_tok = self.cur_token
                self._advance()
                self._expect(T_ARROW)
                if not self._at(T_IDENT):
                    raise CompileError(
                        "expected member name", self.cur_token.line, self.cur_token.col
                    )
                mname_tok = self.cur_token
                mname = mname_tok.value
                self._advance()
                typ, _off = self._lookup(name)
                if typ is None:
                    raise CompileError(
                        f"undefined identifier '{name}'", name_tok.line, name_tok.col
                    )
                if not self._is_struct_ptr(typ):
                    raise CompileError(
                        f"arrow member access on non-struct-pointer '{name}'",
                        name_tok.line,
                        name_tok.col,
                    )
                member = self._lookup_member(
                    self._struct_obj_type(typ), mname, mname_tok.line, mname_tok.col
                )
                if self._at(T_ASSIGN):
                    self._advance()
                    self._emit_load_scalar(name)
                    self.gen.emit("    push ax")
                    self._expr_assign()
                    self.gen.emit("    pop bx")
                    if member.byte_offset:
                        self.gen.emit(f"    add bx, {member.byte_offset}")
                    self._emit_store_member_at_bx(member)
                    return
                self._emit_load_scalar(name)
                self._emit_ptr_member_addr_bx(member)
                self._emit_load_member_at_bx(member)
                self._expr_lor_continue()
                return
            if t2.kind == T_LBRACKET:
                name = self.cur_token.value
                self._advance()
                self._expect(T_LBRACKET)
                self._expr()
                self._expect(T_RBRACKET)
                typ, off = self._lookup(name)
                if typ is None:
                    raise CompileError(f"undefined identifier '{name}'", self.cur_token.line, self.cur_token.col)
                if self._is_struct_type(typ) and name in self.arrays:
                    self._emit_struct_array_elem_addr_bx(name, typ)
                    if not self._at(T_DOT):
                        raise CompileError(
                            "struct array element requires member access",
                            self.cur_token.line,
                            self.cur_token.col,
                        )
                    self._advance()
                    if not self._at(T_IDENT):
                        raise CompileError(
                            "expected member name", self.cur_token.line, self.cur_token.col
                        )
                    mname_tok = self.cur_token
                    mname = mname_tok.value
                    self._advance()
                    member = self._lookup_member(typ, mname, mname_tok.line, mname_tok.col)
                    if member.byte_offset:
                        self.gen.emit(f"    add bx, {member.byte_offset}")
                    if self._at(T_ASSIGN):
                        self._advance()
                        self.gen.emit("    push bx")
                        self._expr_assign()
                        self.gen.emit("    pop bx")
                        self._emit_store_member_at_bx(member)
                        return
                    self._emit_load_member_at_bx(member)
                    self._expr_lor_continue()
                    return
                if self._at(T_ASSIGN):
                    self._advance()
                    if self._is_local(name):
                        self.gen.emit(f"    lea bx, [bp{off:+d}]")
                    else:
                        self.gen.emit(f"    lea bx, [{self._label_for(name)}]")
                    if typ == "int":
                        self.gen.emit("    add ax, ax")
                    self.gen.emit("    add bx, ax")
                    self.gen.emit("    push bx")
                    self._expr_assign()
                    self.gen.emit("    pop bx")
                    if typ == "char":
                        self.gen.emit("    mov [bx], al")
                    else:
                        self.gen.emit("    mov [bx], ax")
                    return
                if self._is_local(name):
                    self.gen.emit(f"    lea bx, [bp{off:+d}]")
                else:
                    self.gen.emit(f"    lea bx, [{self._label_for(name)}]")
                if typ == "int":
                    self.gen.emit("    add ax, ax")
                self.gen.emit("    add bx, ax")
                if typ == "char":
                    self.gen.emit("    xor ah, ah")
                    self.gen.emit("    mov al, [bx]")
                else:
                    self.gen.emit("    mov ax, [bx]")
                self._expr_lor_continue()
                return
        self._expr_ternary()

    def _expr_ternary(self):
        """Ternary cond ? then_expr : else_expr. Precedence between assignment and logical OR."""
        self._expr_lor()
        if self._at(T_QUESTION):
            self._advance()
            l_else = self.gen.new_label()
            l_end = self.gen.new_label()
            self.gen.emit("    cmp ax, 0")
            self.gen.emit(f"    je {l_else}")
            self._expr_assign()
            self.gen.emit(f"    jmp {l_end}")
            self.gen.emit(f"{l_else}:")
            self._expect(T_COLON)
            self._expr_assign()
            self.gen.emit(f"{l_end}:")

    def _expr_lor_continue(self):
        """Continue expression with left value already in ax (after array/member load).

        Handles all binary operators and ternary, matching the precedence chain
        from multiplicative through logical-OR plus ternary.
        """
        while self._at(T_STAR) or self._at(T_SLASH) or self._at(T_PERCENT):
            op = self.cur_token.kind
            self._advance()
            self.gen.emit("    push ax")
            self._expr_unary()
            self.gen.emit("    pop bx")
            if op == T_STAR:
                self.gen.emit("    imul bx")
            elif op == T_SLASH:
                self.gen.emit("    xchg ax, bx")
                self.gen.emit("    cwd")
                self.gen.emit("    idiv bx")
            else:
                self.gen.emit("    xchg ax, bx")
                self.gen.emit("    cwd")
                self.gen.emit("    idiv bx")
                self.gen.emit("    mov ax, dx")
        while self._at(T_PLUS) or self._at(T_MINUS):
            op = self.cur_token.kind
            self._advance()
            self.gen.emit("    push ax")
            self._expr_mul()
            self.gen.emit("    pop bx")
            if op == T_PLUS:
                self.gen.emit("    add ax, bx")
            else:
                self.gen.emit("    sub bx, ax")
                self.gen.emit("    mov ax, bx")
        while self._at(T_SHL) or self._at(T_SHR):
            op = self.cur_token.kind
            self._advance()
            self.gen.emit("    push ax")
            self._expr_add()
            self.gen.emit("    pop bx")
            self.gen.emit("    mov cx, ax")
            self.gen.emit("    mov ax, bx")
            if op == T_SHL:
                self.gen.emit("    shl ax, cl")
            else:
                self.gen.emit("    shr ax, cl")
        while self._at(T_LT) or self._at(T_LE) or self._at(T_GT) or self._at(T_GE):
            op = self.cur_token.kind
            self._advance()
            self.gen.emit("    push ax")
            self._expr_shift()
            self.gen.emit("    pop bx")
            self.gen.emit("    cmp bx, ax")
            l1 = self.gen.new_label()
            l2 = self.gen.new_label()
            jmp_map = {T_LT: "jl", T_LE: "jle", T_GT: "jg", T_GE: "jge"}
            self.gen.emit(f"    {jmp_map[op]} {l1}")
            self.gen.emit("    mov ax, 0")
            self.gen.emit(f"    jmp {l2}")
            self.gen.emit(f"{l1}:")
            self.gen.emit("    mov ax, 1")
            self.gen.emit(f"{l2}:")
        while self._at(T_AND):
            self._advance()
            self.gen.emit("    push ax")
            self._expr_rel()
            self.gen.emit("    pop bx")
            self.gen.emit("    and ax, bx")
        while self._at(T_EQ) or self._at(T_NE):
            op = self.cur_token.kind
            self._advance()
            self.gen.emit("    push ax")
            self._expr_rel()
            self.gen.emit("    pop bx")
            self.gen.emit("    cmp bx, ax")
            l1 = self.gen.new_label()
            l2 = self.gen.new_label()
            if op == T_EQ:
                self.gen.emit(f"    je {l1}")
            else:
                self.gen.emit(f"    jne {l1}")
            self.gen.emit("    mov ax, 0")
            self.gen.emit(f"    jmp {l2}")
            self.gen.emit(f"{l1}:")
            self.gen.emit("    mov ax, 1")
            self.gen.emit(f"{l2}:")
        while self._at(T_XOR):
            self._advance()
            self.gen.emit("    push ax")
            self._expr_eq()
            self.gen.emit("    pop bx")
            self.gen.emit("    xor ax, bx")
        while self._at(T_OR):
            self._advance()
            self.gen.emit("    push ax")
            self._expr_bxor()
            self.gen.emit("    pop bx")
            self.gen.emit("    or ax, bx")
        while self._at(T_LAND):
            self._advance()
            l0 = self.gen.new_label()
            l1 = self.gen.new_label()
            self.gen.emit("    cmp ax, 0")
            self.gen.emit(f"    je {l0}")
            self._expr_bor()
            self.gen.emit("    cmp ax, 0")
            self.gen.emit(f"    je {l0}")
            self.gen.emit("    mov ax, 1")
            self.gen.emit(f"    jmp {l1}")
            self.gen.emit(f"{l0}:")
            self.gen.emit("    mov ax, 0")
            self.gen.emit(f"{l1}:")
        while self._at(T_LOR):
            self._advance()
            ltrue = self.gen.new_label()
            lnext = self.gen.new_label()
            self.gen.emit("    push ax")
            self._expr_land()
            self.gen.emit("    pop bx")
            self.gen.emit("    cmp bx, 0")
            self.gen.emit(f"    jne {ltrue}")
            self.gen.emit(f"    jmp {lnext}")
            self.gen.emit(f"{ltrue}:")
            self.gen.emit("    mov ax, 1")
            self.gen.emit(f"{lnext}:")
        if self._at(T_QUESTION):
            self._advance()
            l_else = self.gen.new_label()
            l_end = self.gen.new_label()
            self.gen.emit("    cmp ax, 0")
            self.gen.emit(f"    je {l_else}")
            self._expr_assign()
            self.gen.emit(f"    jmp {l_end}")
            self.gen.emit(f"{l_else}:")
            self._expect(T_COLON)
            self._expr_assign()
            self.gen.emit(f"{l_end}:")

    def _expr_lor(self):
        self._expr_land()
        while self._at(T_LOR):
            self._advance()
            ltrue = self.gen.new_label()
            lnext = self.gen.new_label()
            self.gen.emit("    push ax")
            self._expr_land()
            self.gen.emit("    pop bx")
            self.gen.emit("    cmp bx, 0")
            self.gen.emit(f"    jne {ltrue}")
            self.gen.emit(f"    jmp {lnext}")
            self.gen.emit(f"{ltrue}:")
            self.gen.emit("    mov ax, 1")
            self.gen.emit(f"{lnext}:")

    def _expr_land(self):
        self._expr_bor()
        while self._at(T_LAND):
            self._advance()
            l0 = self.gen.new_label()
            l1 = self.gen.new_label()
            self.gen.emit("    cmp ax, 0")
            self.gen.emit(f"    je {l0}")
            self._expr_bor()
            self.gen.emit("    cmp ax, 0")
            self.gen.emit(f"    je {l0}")
            self.gen.emit("    mov ax, 1")
            self.gen.emit(f"    jmp {l1}")
            self.gen.emit(f"{l0}:")
            self.gen.emit("    mov ax, 0")
            self.gen.emit(f"{l1}:")

    def _expr_bor(self):
        """Bitwise OR (binary |). Between logical AND and XOR."""
        self._expr_bxor()
        while self._at(T_OR):
            self._advance()
            self.gen.emit("    push ax")
            self._expr_bxor()
            self.gen.emit("    pop bx")
            self.gen.emit("    or ax, bx")

    def _expr_bxor(self):
        """Bitwise XOR (binary ^). Between | and ==."""
        self._expr_eq()
        while self._at(T_XOR):
            self._advance()
            self.gen.emit("    push ax")
            self._expr_eq()
            self.gen.emit("    pop bx")
            self.gen.emit("    xor ax, bx")

    def _expr_eq(self):
        self._expr_band()
        while self._at(T_EQ) or self._at(T_NE):
            op = self.cur_token.kind
            self._advance()
            self.gen.emit("    push ax")
            self._expr_rel()
            self.gen.emit("    pop bx")
            self.gen.emit("    cmp bx, ax")
            l1 = self.gen.new_label()
            l2 = self.gen.new_label()
            if op == T_EQ:
                self.gen.emit(f"    je {l1}")
            else:
                self.gen.emit(f"    jne {l1}")
            self.gen.emit("    mov ax, 0")
            self.gen.emit(f"    jmp {l2}")
            self.gen.emit(f"{l1}:")
            self.gen.emit("    mov ax, 1")
            self.gen.emit(f"{l2}:")

    def _expr_band(self):
        """Bitwise AND (binary &). Precedence between == and relational."""
        self._expr_rel()
        while self._at(T_AND):
            self._advance()
            self.gen.emit("    push ax")
            self._expr_rel()
            self.gen.emit("    pop bx")
            self.gen.emit("    and ax, bx")

    def _expr_rel(self):
        self._expr_shift()
        while self._at(T_LT) or self._at(T_LE) or self._at(T_GT) or self._at(T_GE):
            op = self.cur_token.kind
            self._advance()
            self.gen.emit("    push ax")
            self._expr_shift()
            self.gen.emit("    pop bx")
            self.gen.emit("    cmp bx, ax")
            l1 = self.gen.new_label()
            l2 = self.gen.new_label()
            jmp_map = {T_LT: "jl", T_LE: "jle", T_GT: "jg", T_GE: "jge"}
            self.gen.emit(f"    {jmp_map[op]} {l1}")
            self.gen.emit("    mov ax, 0")
            self.gen.emit(f"    jmp {l2}")
            self.gen.emit(f"{l1}:")
            self.gen.emit("    mov ax, 1")
            self.gen.emit(f"{l2}:")

    def _expr_shift(self):
        self._expr_add()
        while self._at(T_SHL) or self._at(T_SHR):
            op = self.cur_token.kind
            self._advance()
            self.gen.emit("    push ax")
            self._expr_add()
            self.gen.emit("    pop bx")
            self.gen.emit("    mov cx, ax")
            self.gen.emit("    mov ax, bx")
            if op == T_SHL:
                self.gen.emit("    shl ax, cl")
            else:
                self.gen.emit("    shr ax, cl")

    def _expr_add(self):
        self._expr_mul()
        while self._at(T_PLUS) or self._at(T_MINUS):
            op = self.cur_token.kind
            self._advance()
            self.gen.emit("    push ax")
            self._expr_mul()
            self.gen.emit("    pop bx")
            if op == T_PLUS:
                self.gen.emit("    add ax, bx")
            else:
                self.gen.emit("    sub bx, ax")
                self.gen.emit("    mov ax, bx")

    def _expr_mul(self):
        self._expr_unary()
        while self._at(T_STAR) or self._at(T_SLASH) or self._at(T_PERCENT):
            op = self.cur_token.kind
            self._advance()
            self.gen.emit("    push ax")
            self._expr_unary()
            self.gen.emit("    pop bx")
            if op == T_STAR:
                self.gen.emit("    imul bx")
            elif op == T_SLASH:
                self.gen.emit("    xchg ax, bx")
                self.gen.emit("    cwd")
                self.gen.emit("    idiv bx")
            else:
                self.gen.emit("    xchg ax, bx")
                self.gen.emit("    cwd")
                self.gen.emit("    idiv bx")
                self.gen.emit("    mov ax, dx")

    def _expr_unary(self):
        if self._at(T_SIZEOF):
            tok = self.cur_token
            self._advance()
            if self._at(T_LPAREN):
                self._advance()
                if self._is_cast_type_start():
                    typ = self._parse_sizeof_type()
                    self._expect(T_RPAREN)
                    sz = self._sizeof_value(typ, tok.line, tok.col)
                else:
                    sz = self._sizeof_expr_no_eval()
                    self._expect(T_RPAREN)
            else:
                sz = self._sizeof_expr_no_eval()
            self.gen.emit(f"    mov ax, {sz}")
            self.last_primary_type = "int"
            return
        if self._at(T_LPAREN):
            # Cast: (type)unary  vs parenthesized expr handled in primary.
            t2 = self._peek_next()
            if (
                t2.kind == T_INT
                or t2.kind == T_CHAR
                or t2.kind == T_VOID
                or t2.kind == T_STRUCT
                or t2.kind == T_ENUM
            ):
                self._advance()
                typ = self._parse_sizeof_type()
                self._expect(T_RPAREN)
                self._expr_unary()
                self._emit_cast(typ)
                return
        if self._at(T_MINUS):
            self._advance()
            self._expr_unary()
            self.gen.emit("    neg ax")
            return
        if self._at(T_LNOT):
            self._advance()
            self._expr_unary()
            l0 = self.gen.new_label()
            l1 = self.gen.new_label()
            self.gen.emit("    cmp ax, 0")
            self.gen.emit(f"    je {l0}")
            self.gen.emit("    mov ax, 0")
            self.gen.emit(f"    jmp {l1}")
            self.gen.emit(f"{l0}:")
            self.gen.emit("    mov ax, 1")
            self.gen.emit(f"{l1}:")
            return
        if self._at(T_NOT):
            self._advance()
            self._expr_unary()
            self.gen.emit("    not ax")
            return
        if self._at(T_AND):
            self._advance()
            self._expr_primary_addr()
            return
        if self._at(T_STAR):
            self._advance()
            self._expr_unary()
            if self.last_primary_type == "void*":
                raise CompileError(
                    "cannot dereference void pointer",
                    self.cur_token.line,
                    self.cur_token.col,
                )
            if self._is_struct_ptr(self.last_primary_type):
                raise CompileError(
                    "cannot dereference struct pointer (use ->)",
                    self.cur_token.line,
                    self.cur_token.col,
                )
            # dereference: load word at [ax]
            self.gen.emit("    mov bx, ax")
            self.gen.emit("    mov ax, [bx]")
            self.last_primary_type = "int"
            return
        self._expr_postfix()

    def _expr_postfix(self):
        self._expr_primary()
        while self._at(T_LPAREN):
            self._advance()
            # Function call: ax has function address? No, we call by name. So primary for call is ident followed by (. So we need to handle f(...) in primary.
            args = []
            while not self._at(T_RPAREN):
                self._expr()
                args.append(1)
                if not self._at(T_RPAREN):
                    self._expect(T_COMMA)
            self._expect(T_RPAREN)
            for _ in args:
                self.gen.emit("    add sp, 2")
            # Actually we push args right-to-left, then call. So we need the function name in primary when we see (. Let me handle in primary: ident then ( => call.
            # So in primary: if we have ident and next is (, then it's a call. Push args right-to-left, call name, add sp 2*n.
            # For now skip this loop and only support call from primary.
        if self._at(T_LBRACKET):
            self._advance()
            elt_type = self.last_primary_type
            self.gen.emit("    push ax")
            self._expr()
            self._expect(T_RBRACKET)
            if self._is_struct_type(elt_type):
                st = self._require_complete_struct(
                    elt_type, self.cur_token.line, self.cur_token.col
                )
                sz = st.size
                self.gen.emit("    mov cx, ax")
                if sz <= 1:
                    self.gen.emit("    mov ax, cx")
                else:
                    self.gen.emit(f"    mov ax, {sz}")
                    self.gen.emit("    mul cx")
                self.gen.emit("    pop bx")
                self.gen.emit("    add bx, ax")
                if not self._at(T_DOT):
                    raise CompileError(
                        "struct array element requires member access",
                        self.cur_token.line,
                        self.cur_token.col,
                    )
                self._advance()
                if not self._at(T_IDENT):
                    raise CompileError(
                        "expected member name", self.cur_token.line, self.cur_token.col
                    )
                mname_tok = self.cur_token
                mname = mname_tok.value
                self._advance()
                member = self._lookup_member(elt_type, mname, mname_tok.line, mname_tok.col)
                if member.byte_offset:
                    self.gen.emit(f"    add bx, {member.byte_offset}")
                self._emit_load_member_at_bx(member)
                return
            if elt_type != "char":
                self.gen.emit("    add ax, ax")
            self.gen.emit("    pop bx")
            self.gen.emit("    add bx, ax")
            if elt_type == "char":
                self.gen.emit("    xor ah, ah")
                self.gen.emit("    mov al, [bx]")
            else:
                self.gen.emit("    mov ax, [bx]")

    def _expr_primary_addr(self):
        """Parse lvalue (ident, ident[expr], ident.member, ident->member) into AX."""
        if not self._at(T_IDENT):
            raise CompileError("expected identifier for address-of", self.cur_token.line, self.cur_token.col)
        name = self.cur_token.value
        self._advance()
        typ, off = self._lookup(name)
        if typ is None:
            raise CompileError(f"undefined identifier '{name}'", self.cur_token.line, self.cur_token.col)
        if self._at(T_DOT):
            self._advance()
            if not self._at(T_IDENT):
                raise CompileError(
                    "expected member name", self.cur_token.line, self.cur_token.col
                )
            mname_tok = self.cur_token
            mname = mname_tok.value
            self._advance()
            if not self._is_struct_type(typ):
                raise CompileError(
                    f"dot member access on non-struct '{name}'",
                    mname_tok.line,
                    mname_tok.col,
                )
            member = self._lookup_member(typ, mname, mname_tok.line, mname_tok.col)
            if member.is_bitfield:
                raise CompileError(
                    "cannot take address of bit-field", mname_tok.line, mname_tok.col
                )
            self._emit_member_addr_bx(name, member)
            self.gen.emit("    mov ax, bx")
            return
        if self._at(T_ARROW):
            self._advance()
            if not self._at(T_IDENT):
                raise CompileError(
                    "expected member name", self.cur_token.line, self.cur_token.col
                )
            mname_tok = self.cur_token
            mname = mname_tok.value
            self._advance()
            if not self._is_struct_ptr(typ):
                raise CompileError(
                    f"arrow member access on non-struct-pointer '{name}'",
                    mname_tok.line,
                    mname_tok.col,
                )
            member = self._lookup_member(
                self._struct_obj_type(typ), mname, mname_tok.line, mname_tok.col
            )
            if member.is_bitfield:
                raise CompileError(
                    "cannot take address of bit-field", mname_tok.line, mname_tok.col
                )
            self._emit_load_scalar(name)
            self._emit_ptr_member_addr_bx(member)
            self.gen.emit("    mov ax, bx")
            return
        if self._at(T_LBRACKET):
            self._advance()
            if self._is_local(name):
                self.gen.emit(f"    lea ax, [bp{off:+d}]")
            else:
                self.gen.emit(f"    lea ax, [{self._label_for(name)}]")
            self.gen.emit("    push ax")
            self._expr()
            self._expect(T_RBRACKET)
            if self._is_struct_type(typ):
                st = self._require_complete_struct(typ, self.cur_token.line, self.cur_token.col)
                sz = st.size
                self.gen.emit("    mov cx, ax")
                if sz <= 1:
                    self.gen.emit("    mov ax, cx")
                else:
                    self.gen.emit(f"    mov ax, {sz}")
                    self.gen.emit("    mul cx")
                self.gen.emit("    pop bx")
                self.gen.emit("    add bx, ax")
                self.gen.emit("    mov ax, bx")
            else:
                self.gen.emit("    add ax, ax")
                self.gen.emit("    pop bx")
                self.gen.emit("    add bx, ax")
                self.gen.emit("    mov ax, bx")
        else:
            if self._is_struct_type(typ):
                # Address of whole struct
                if self._is_local(name):
                    self.gen.emit(f"    lea ax, [bp{off:+d}]")
                else:
                    self.gen.emit(f"    lea ax, [{self._label_for(name)}]")
            elif self._is_local(name):
                self.gen.emit(f"    lea ax, [bp{off:+d}]")
            else:
                self.gen.emit(f"    lea ax, [{self._label_for(name)}]")

    def _expr_primary(self):
        if self._at(T_NUMBER):
            v = self.cur_token.value
            self._advance()
            self.gen.emit(f"    mov ax, {v}")
            self.last_primary_type = "int"
            return
        if self._at(T_STRING):
            s = self.cur_token.value
            self._advance()
            label = self.gen.add_string(s)
            self.gen.emit(f"    lea ax, [{label}]")
            self.last_primary_type = "char*"
            return
        if self._at(T_IDENT):
            name = self.cur_token.value
            self._advance()
            if self._at(T_LPAREN):
                self._advance()
                nargs = 0
                while not self._at(T_RPAREN):
                    self._expr()
                    self.gen.emit("    push ax")
                    nargs += 1
                    if not self._at(T_RPAREN):
                        self._expect(T_COMMA)
                self._expect(T_RPAREN)
                self.gen.emit(f"    call {name}")
                self.gen.emit(f"    add sp, {2 * nargs}")
                self.last_primary_type = "int"
                return
            typ, off = self._lookup(name)
            if typ is None:
                if name in self.enumerators:
                    self.gen.emit(f"    mov ax, {self.enumerators[name] & 0xFFFF}")
                    self.last_primary_type = "int"
                    return
                raise CompileError(f"undefined identifier '{name}'", self.cur_token.line, self.cur_token.col)
            if self._at(T_DOT):
                self._advance()
                if not self._at(T_IDENT):
                    raise CompileError(
                        "expected member name", self.cur_token.line, self.cur_token.col
                    )
                mname_tok = self.cur_token
                mname = mname_tok.value
                self._advance()
                if not self._is_struct_type(typ):
                    raise CompileError(
                        f"dot member access on non-struct '{name}'",
                        mname_tok.line,
                        mname_tok.col,
                    )
                member = self._lookup_member(typ, mname, mname_tok.line, mname_tok.col)
                self._emit_load_member(name, member)
                return
            if self._at(T_ARROW):
                self._advance()
                if not self._at(T_IDENT):
                    raise CompileError(
                        "expected member name", self.cur_token.line, self.cur_token.col
                    )
                mname_tok = self.cur_token
                mname = mname_tok.value
                self._advance()
                if not self._is_struct_ptr(typ):
                    raise CompileError(
                        f"arrow member access on non-struct-pointer '{name}'",
                        mname_tok.line,
                        mname_tok.col,
                    )
                member = self._lookup_member(
                    self._struct_obj_type(typ), mname, mname_tok.line, mname_tok.col
                )
                self._emit_load_scalar(name)
                self._emit_ptr_member_addr_bx(member)
                self._emit_load_member_at_bx(member)
                return
            if self._at(T_LBRACKET):
                self.last_primary_type = typ
                if self._is_local(name):
                    self.gen.emit(f"    lea ax, [bp{off:+d}]")
                else:
                    self.gen.emit(f"    lea ax, [{self._label_for(name)}]")
            else:
                # Array names decay to pointers (address); scalars load value.
                if name in self.arrays:
                    if self._is_local(name):
                        self.gen.emit(f"    lea ax, [bp{off:+d}]")
                    else:
                        self.gen.emit(f"    lea ax, [{self._label_for(name)}]")
                    self.last_primary_type = typ
                elif self._is_struct_type(typ):
                    raise CompileError(
                        "struct values are not supported (use member access)",
                        self.cur_token.line,
                        self.cur_token.col,
                    )
                elif self._is_local(name):
                    self.gen.emit(f"    mov ax, [bp{off:+d}]")
                    self.last_primary_type = typ
                else:
                    self.gen.emit(f"    mov ax, [{self._label_for(name)}]")
                    self.last_primary_type = typ
            return
        if self._at(T_LPAREN):
            self._advance()
            self._expr()
            self._expect(T_RPAREN)
            return
        raise CompileError(f"expected expression, got {self.cur_token.kind}", self.cur_token.line, self.cur_token.col)


def main():
    ap = argparse.ArgumentParser(description="rmcc: Small-C compiler for rmDOS")
    ap.add_argument("input", type=Path, help="Input .c file")
    ap.add_argument("-o", "--output", type=Path, required=True, help="Output .s file")
    ap.add_argument("--module", type=str, default=None, help="Emit MOD0 module with this name (e.g. HALT)")
    ap.add_argument("--com", action="store_true", help="Emit DOS .COM entry (_start + INT 21h/4Ch)")
    ap.add_argument(
        "--overlay",
        action="store_true",
        help="Emit DOS overlay entry (_start + retf; for INT 21h/4B03)",
    )
    ap.add_argument(
        "--exe",
        action="store_true",
        help="Emit small-model MZ entry (DS=SS, zero BSS, INT 21h/4Ch)",
    )
    ap.add_argument("-I", "--include", action="append", default=[], dest="include_dirs", help="Include path for #include")
    ap.add_argument("--target", choices=("gas", "wasm"), default="gas", help="Assembly target: gas (GNU as) or wasm (tools/asm/wasm)")
    args = ap.parse_args()
    modes = sum(1 for f in (args.com, args.exe, args.overlay, bool(args.module)) if f)
    if modes > 1:
        ap.error("--com, --exe, --overlay, and --module are mutually exclusive")
    try:
        src = args.input.read_text()
    except FileNotFoundError:
        raise SystemExit(f"source file not found: {args.input}")
    if args.include_dirs:
        inc_dirs = [Path(d) for d in args.include_dirs]
    else:
        inc_dirs = default_include_dirs(args.input)
    src = preprocess_includes(src, inc_dirs)

    out_path = args.output
    try:
        comp = Compiler(
            src,
            filename=str(args.input),
            module_name=args.module,
            com_entry=args.com,
            exe_entry=args.exe,
            overlay_entry=args.overlay,
        )
        comp.include_paths = inc_dirs
        asm = comp.compile()
        if args.target == "wasm":
            asm = "# wasm-compatible\n" + asm
        out_path.write_text(asm)
    except CompileError as e:
        print(f"{args.input}:{e.line}:{e.col}: {e.msg}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
