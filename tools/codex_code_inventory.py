from __future__ import annotations

import argparse
import re
from pathlib import Path


FUNC_RE = re.compile(
    r"(?m)^(?!\s*(?:if|for|while|switch)\s*\()"
    r"(?P<prefix>(?:static\s+)?(?:inline\s+)?(?:const\s+)?[A-Za-z_][\w\s\*]*?)"
    r"\s+(?P<name>[A-Za-z_]\w*)\s*\((?P<args>[^;{}]*?)\)\s*\{"
)
TYPE_RE = re.compile(r"(?m)^\s*typedef\s+(?:struct|enum|union)\s+([A-Za-z_]\w*)?")
DEFINE_RE = re.compile(r"(?m)^\s*#define\s+([A-Za-z_]\w*)\s+(.+)$")


def preceding_comment(text: str, start: int) -> str:
    prefix = text[:start].rstrip()
    if prefix.endswith("*/"):
        begin = prefix.rfind("/*")
        if begin >= 0:
            body = prefix[begin + 2 : -2]
            return " ".join(line.strip(" *\t") for line in body.splitlines()).strip()
    lines = prefix.splitlines()
    comments = []
    for line in reversed(lines[-6:]):
        stripped = line.strip()
        if stripped.startswith("//"):
            comments.append(stripped[2:].strip())
        elif stripped == "":
            if comments:
                break
        else:
            break
    return " ".join(reversed(comments))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    lines = []
    for path in sorted(args.root.rglob("*")):
        if path.suffix.lower() not in {".c", ".h"}:
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            text = path.read_text(encoding="gb18030", errors="replace")
        rel = path.relative_to(args.root)
        lines.append(f"\n## {rel}")
        for match in FUNC_RE.finditer(text):
            comment = preceding_comment(text, match.start())
            prefix = " ".join(match.group("prefix").split())
            fn_args = " ".join(match.group("args").split())
            lines.append(f"FUNC\t{match.group('name')}\t{prefix}\t{fn_args}\t{comment}")
        for match in TYPE_RE.finditer(text):
            if match.group(1):
                lines.append(f"TYPE\t{match.group(1)}")
        for match in DEFINE_RE.finditer(text):
            value = match.group(2).strip()
            if len(value) <= 140:
                lines.append(f"DEFINE\t{match.group(1)}\t{value}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="utf-8")


if __name__ == "__main__":
    main()
