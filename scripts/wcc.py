#!/usr/bin/env python3
"""
WCC: Small-C compiler for rmDOS.
Compiles a C subset to 8088 GAS assembly.
Usage: python3 -m scripts.wcc [options] input.c -o output.s
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from scripts.wcc_preprocess import default_include_dirs, preprocess_includes

# --- Token types ---
T_INT, T_CHAR, T_VOID = "INT", "CHAR", "VOID"
T_IF, T_ELSE, T_WHILE, T_FOR, T_RETURN, T_ASM, T_BREAK, T_CONTINUE = (
    "IF", "ELSE", "WHILE", "FOR", "RETURN", "ASM", "BREAK", "CONTINUE",
)
T_IDENT, T_NUMBER, T_STRING = "IDENT", "NUMBER", "STRING"
T_LBRACE, T_RBRACE, T_LPAREN, T_RPAREN, T_LBRACKET, T_RBRACKET = "LBRACE", "RBRACE", "LPAREN", "RPAREN", "LBRACKET", "RBRACKET"
T_SEMI, T_COMMA = "SEMI", "COMMA"
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
    "int": T_INT, "char": T_CHAR, "void": T_VOID,
    "if": T_IF, "else": T_ELSE, "while": T_WHILE, "for": T_FOR,
    "return": T_RETURN, "asm": T_ASM, "break": T_BREAK, "continue": T_CONTINUE,
}


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

        # Single char
        single = {
            "{": T_LBRACE, "}": T_RBRACE, "(": T_LPAREN, ")": T_RPAREN,
            "[": T_LBRACKET, "]": T_RBRACKET, ";": T_SEMI, ",": T_COMMA,
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
    ):
        self.lexer = Lexer(source, filename)
        self.gen = CodeGen()
        self.module_name = module_name
        self.com_entry = com_entry
        self.cur_token: Token | None = None
        self.globals: dict[str, tuple[str, int]] = {}  # name -> (type, size) size=0 for scalar
        self.locals: dict[str, tuple[str, int]] = {}  # name -> (type, bp_offset)
        self.params: list[tuple[str, str]] = []  # (name, type)
        self.local_offset = 0
        self.last_primary_type = "int"
        self.include_paths: list[Path] = []
        self.break_labels: list[str] = []
        self.continue_labels: list[str] = []
        self.arrays: set[str] = set()  # names that decay to pointers

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
        if name in self.globals:
            return self.globals[name]
        return None, 0

    def _is_local(self, name: str) -> bool:
        return name in self.locals

    def compile(self) -> str:
        self._advance()
        self.gen.emit(".code16")
        self.gen.emit(".intel_syntax noprefix")
        self.gen.emit(".section .text")

        if self.com_entry:
            # DOS .COM: linked at 0x100, CS=DS=ES=PSP segment.
            self.gen.emit(".global _start")
            self.gen.emit("_start:")
            self.gen.emit("    push cs")
            self.gen.emit("    pop ds")
            self.gen.emit("    push cs")
            self.gen.emit("    pop es")
            self.gen.emit("    call main")
            self.gen.emit("    mov ah, 0x4C")
            self.gen.emit("    int 0x21")
        elif self.module_name:
            self.gen.emit(".global module_header")
            self.gen.emit("module_header:")
            self.gen.emit('    .ascii "MOD0"')
            self.gen.emit("    .word module_entry")
            self.gen.emit(f'    .ascii "{self.module_name}"')
            self.gen.emit("    .byte 0x00")
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
            # Optional storage class (ignored)
            if self._at(T_IDENT) and self.cur_token.value == "static":
                self._advance()
            t = self._type_spec()
            if t is None:
                break
            if not self._at(T_IDENT):
                raise CompileError("expected identifier", self.cur_token.line, self.cur_token.col)
            next_t = self._peek_next()
            if next_t.kind != T_LPAREN:
                # Global variable (scalar or array), optional initializer
                name = self.cur_token.value
                self._advance()
                size = 0
                if self._at(T_LBRACKET):
                    self._advance()
                    size = self._expect(T_NUMBER).value
                    self._expect(T_RBRACKET)
                has_init = self._at(T_ASSIGN)
                if has_init:
                    self._advance()  # consume =
                self.globals[name] = (t, size)
                if size > 0:
                    self.arrays.add(name)
                self.gen.emit(".section .data")
                self.gen.emit(f".global {name}")
                self.gen.emit(f"{name}:")
                if has_init:
                    if size > 0:
                        elem_size = 2 if t == "int" else 1
                        if self._at(T_LBRACE):
                            self._advance()
                            values = []
                            while not self._at(T_RBRACE) and not self._at(T_EOF):
                                if not self._at(T_NUMBER):
                                    raise CompileError("brace initializer element must be a number", self.cur_token.line, self.cur_token.col)
                                values.append(self.cur_token.value & (0xFFFF if t == "int" else 0xFF))
                                self._advance()
                                if not self._at(T_RBRACE):
                                    self._expect(T_COMMA)
                            self._expect(T_RBRACE)
                            if not values or (len(values) == 1 and values[0] == 0):
                                self.gen.emit(f"    .space {size * elem_size}, 0")
                            else:
                                if len(values) > size:
                                    raise CompileError(f"too many initializers (have {len(values)}, array size {size})", self.cur_token.line, self.cur_token.col)
                                if t == "int":
                                    self.gen.emit("    .word " + ", ".join(str(v) for v in values))
                                else:
                                    self.gen.emit("    .byte " + ", ".join(str(v) for v in values))
                                if len(values) < size:
                                    self.gen.emit(f"    .space {(size - len(values)) * elem_size}, 0")
                        elif self._at(T_STRING):
                            # char buf[N] = "string"
                            if t != "char":
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
                        # scalar: int x = 5; or char c = 'a';
                        if t == "int":
                            if not self._at(T_NUMBER):
                                raise CompileError("int initializer must be a number", self.cur_token.line, self.cur_token.col)
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
                        elem_size = 2 if t == "int" else 1
                        self.gen.emit(f"    .space {size * elem_size}, 0")
                    else:
                        if t == "int":
                            self.gen.emit("    .word 0")
                        else:
                            self.gen.emit("    .byte 0")
                self._expect(T_SEMI)
                self.gen.emit(".section .text")
                continue

            # Function
            name = self._expect(T_IDENT).value
            self._expect(T_LPAREN)
            params: list[tuple[str, str]] = []
            while not self._at(T_RPAREN):
                pt = self._type_spec()
                if pt is None:
                    break
                if pt == "void" and self._at(T_RPAREN):
                    break
                if self._at(T_STAR):
                    self._advance()
                    pt = pt + "*"
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
            self.local_offset = 0
            # Drop previous function's local arrays; keep global arrays.
            self.arrays = {n for n, (_t, sz) in self.globals.items() if sz > 0}
            # Args pushed left-to-right: last arg at [bp+4], first at [bp+4+2*(n-1)]
            for i, (pname, _) in enumerate(params):
                self.locals[pname] = ("int", 4 + 2 * (len(params) - 1 - i))

            # Emit function prologue, then body, then epilogue
            self.gen.emit(f".global {name}")
            self.gen.emit(f"{name}:")
            self.gen.emit("    push bp")
            self.gen.emit("    mov bp, sp")
            self._parse_statements()
            self._expect(T_RBRACE)
            self.gen.emit("    mov sp, bp")
            self.gen.emit("    pop bp")
            self.gen.emit("    ret")

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
        t = self._type_spec()
        if t is not None:
            while True:
                name = self._expect(T_IDENT).value
                size = 0
                if self._at(T_LBRACKET):
                    self._advance()
                    size = self._expect(T_NUMBER).value
                    self._expect(T_RBRACKET)
                if size > 0:
                    self.gen.emit(f"    sub sp, {size}")
                    self.local_offset += size
                    self.locals[name] = (t, -self.local_offset)
                    self.arrays.add(name)
                else:
                    sz = 2 if t == "int" else 1
                    self.gen.emit(f"    sub sp, {sz}")
                    self.local_offset += sz
                    self.locals[name] = (t, -self.local_offset)
                if not self._at(T_COMMA):
                    break
                self._advance()
            self._expect(T_SEMI)
            return
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
                self.lexer._pending = list(incr_tokens) + self.lexer._pending
                self._advance()
                self._expr()
                self.lexer.push_back(saved)
                self._advance()
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
                self._expr_assign()
                typ, off = self._lookup(name)
                if typ is None:
                    raise CompileError(f"undefined identifier '{name}'", self.cur_token.line, self.cur_token.col)
                if self._is_local(name):
                    self.gen.emit(f"    mov [bp{off:+d}], ax")
                else:
                    self.gen.emit(f"    mov [{name}], ax")
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
                if self._at(T_ASSIGN):
                    self._advance()
                    if self._is_local(name):
                        self.gen.emit(f"    lea bx, [bp{off:+d}]")
                    else:
                        self.gen.emit(f"    lea bx, [{name}]")
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
                    self.gen.emit(f"    lea bx, [{name}]")
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
        """Continue expression parsing with left value already in ax (e.g. after array load)."""
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
        while self._at(T_AND):
            self._advance()
            self.gen.emit("    push ax")
            self._expr_rel()
            self.gen.emit("    pop bx")
            self.gen.emit("    and ax, bx")
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
            # dereference: load word at [ax]
            self.gen.emit("    mov bx, ax")
            self.gen.emit("    mov ax, [bx]")
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
        """Parse lvalue (ident or ident[expr]) and emit its address into AX."""
        if not self._at(T_IDENT):
            raise CompileError("expected identifier for address-of", self.cur_token.line, self.cur_token.col)
        name = self.cur_token.value
        self._advance()
        typ, off = self._lookup(name)
        if typ is None:
            raise CompileError(f"undefined identifier '{name}'", self.cur_token.line, self.cur_token.col)
        if self._at(T_LBRACKET):
            self._advance()
            if self._is_local(name):
                self.gen.emit(f"    lea ax, [bp{off:+d}]")
            else:
                self.gen.emit(f"    lea ax, [{name}]")
            self.gen.emit("    push ax")
            self._expr()
            self._expect(T_RBRACKET)
            self.gen.emit("    add ax, ax")
            self.gen.emit("    pop bx")
            self.gen.emit("    add bx, ax")
            self.gen.emit("    mov ax, bx")
        else:
            if self._is_local(name):
                self.gen.emit(f"    lea ax, [bp{off:+d}]")
            else:
                self.gen.emit(f"    lea ax, [{name}]")

    def _expr_primary(self):
        if self._at(T_NUMBER):
            v = self.cur_token.value
            self._advance()
            self.gen.emit(f"    mov ax, {v}")
            return
        if self._at(T_STRING):
            s = self.cur_token.value
            self._advance()
            label = self.gen.add_string(s)
            self.gen.emit(f"    lea ax, [{label}]")
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
                return
            typ, off = self._lookup(name)
            if typ is None:
                raise CompileError(f"undefined identifier '{name}'", self.cur_token.line, self.cur_token.col)
            if self._at(T_LBRACKET):
                self.last_primary_type = typ
                if self._is_local(name):
                    self.gen.emit(f"    lea ax, [bp{off:+d}]")
                else:
                    self.gen.emit(f"    lea ax, [{name}]")
            else:
                # Array names decay to pointers (address); scalars load value.
                if name in self.arrays:
                    if self._is_local(name):
                        self.gen.emit(f"    lea ax, [bp{off:+d}]")
                    else:
                        self.gen.emit(f"    lea ax, [{name}]")
                elif self._is_local(name):
                    self.gen.emit(f"    mov ax, [bp{off:+d}]")
                else:
                    self.gen.emit(f"    mov ax, [{name}]")
            return
        if self._at(T_LPAREN):
            self._advance()
            self._expr()
            self._expect(T_RPAREN)
            return
        raise CompileError(f"expected expression, got {self.cur_token.kind}", self.cur_token.line, self.cur_token.col)


def main():
    ap = argparse.ArgumentParser(description="WCC: Small-C compiler for rmDOS")
    ap.add_argument("input", type=Path, help="Input .c file")
    ap.add_argument("-o", "--output", type=Path, required=True, help="Output .s file")
    ap.add_argument("--module", type=str, default=None, help="Emit MOD0 module with this name (e.g. HALT)")
    ap.add_argument("--com", action="store_true", help="Emit DOS .COM entry (_start + INT 21h/4Ch)")
    ap.add_argument("-I", "--include", action="append", default=[], dest="include_dirs", help="Include path for #include")
    ap.add_argument("--target", choices=("gas", "wasm"), default="gas", help="Assembly target: gas (GNU as) or wasm (tools/asm/wasm)")
    args = ap.parse_args()
    if args.com and args.module:
        ap.error("--com and --module are mutually exclusive")
    src = args.input.read_text()
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
