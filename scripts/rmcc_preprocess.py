from __future__ import annotations

from pathlib import Path


def default_include_dirs(input_path: Path) -> list[Path]:
    root = Path(__file__).parent.parent
    return [
        input_path.parent,
        root / "firmware" / "src" / "dos" / "inc",
    ]


def preprocess_includes(source: str, include_dirs: list[Path]) -> str:
    """Expand one-level #include "path" directives before lexing."""
    out_lines: list[str] = []
    for line in source.splitlines():
        stripped = line.strip()
        if stripped.startswith('#include "') and stripped.endswith('"'):
            include_path = stripped[10:-1]
            for include_dir in include_dirs:
                candidate = include_dir / include_path
                if candidate.exists():
                    out_lines.append(candidate.read_text())
                    break
            else:
                raise FileNotFoundError(f"include not found: {include_path}")
        else:
            out_lines.append(line)
    return "\n".join(out_lines)
