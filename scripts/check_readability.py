"""Reject ternaries, compressed C++ declarations and formatting drift.

Checks cover first-party code only. The formatter enforces explicit braces and
expanded blocks/lambdas. Naming, units and algorithm clarity still need review.
"""

import ast
import re
import shutil
import subprocess
import sys
from pathlib import Path

CPP_NON_CODE = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
CPP_CHAINED_ASSIGNMENT = re.compile(
    r"\b\w+(?:->\w+|\.\w+)*\s*=(?!=)\s*\w+(?:->\w+|\.\w+)*\s*=(?!=)"
)
CPP_DECLARATION = re.compile(
    r"^\s*(?:inline\s+)?(?:(?:const|constexpr)\s+)*(?:bool|float|double|int32|int|FKey)\s+(.+);\s*$"
)


def cpp_code_only(text):
    """Retain line positions while ignoring punctuation inside comments/literals."""

    def blank(match):
        characters = []
        for character in match.group():
            if character == "\n":
                characters.append("\n")
            else:
                characters.append(" ")
        return "".join(characters)

    return CPP_NON_CODE.sub(blank, text)


def cpp_ternary_lines(text):
    code = cpp_code_only(text)
    return [number for number, line in enumerate(code.splitlines(), 1) if "?" in line]


def has_multiple_declarations(line):
    """A comma inside a call/initializer is valid; top-level declarator commas are not."""
    match = CPP_DECLARATION.match(line)
    if not match:
        return False
    depth = 0
    for character in match.group(1):
        if character in "([{":
            depth += 1
        elif character in ")]}":
            depth -= 1
        elif character == "," and depth == 0:
            return True
    return False


def formatter_path():
    """Find the pinned dev tool even when the virtual environment is not activated."""
    executable = "clang-format"
    if sys.platform == "win32":
        executable += ".exe"
    local = Path(sys.executable).parent / executable
    if local.is_file():
        return str(local)
    discovered = shutil.which(executable)
    if not discovered:
        raise SystemExit("Install dev tools first: python -m pip install -e '.[dev]'")
    return discovered


def main():
    failures = []
    for root in ["src", "scripts"]:
        for path in Path(root).rglob("*.py"):
            tree = ast.parse(path.read_text())
            for node in ast.walk(tree):
                if isinstance(node, ast.IfExp):
                    failures.append(f"{path}:{node.lineno}: use explicit if/else")
    cpp_paths = []
    for root in ["Source", "tests/cpp"]:
        for path in Path(root).rglob("*"):
            if path.suffix not in [".cpp", ".h"]:
                continue
            cpp_paths.append(str(path))
            text = path.read_text()
            for line in cpp_ternary_lines(text):
                failures.append(f"{path}:{line}: use explicit if/else")
            for number, line in enumerate(cpp_code_only(text).splitlines(), 1):
                if CPP_CHAINED_ASSIGNMENT.search(line):
                    failures.append(f"{path}:{number}: write assignments separately")
                if has_multiple_declarations(line):
                    failures.append(f"{path}:{number}: declare one variable per statement")
    if failures:
        raise SystemExit("\n".join(failures))
    if cpp_paths:
        result = subprocess.run(
            [formatter_path(), "--dry-run", "--Werror", *sorted(cpp_paths)], check=False
        )
        if result.returncode:
            raise SystemExit("Format C++ with the repository .clang-format before continuing.")
    print("Readability guard passed: explicit C++ style; no ternaries or chained assignments.")


if __name__ == "__main__":
    main()
