#!/usr/bin/env python3
"""生成普通访问与 ONCE 访问的汇编，供人工比较。"""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "src" / "access_once.c"
GENERATED = ROOT / "generated"


def compiler_version(executable: str) -> str:
    result = subprocess.run(
        [executable, "--version"],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.splitlines()[0]


def generate(executable: str, name: str, optimization: str) -> None:
    output = GENERATED / f"{name}_{optimization}.s"
    command = [
        executable,
        f"-{optimization}",
        "-S",
        "-fno-asynchronous-unwind-tables",
        "-fno-ident",
        str(SOURCE),
        "-o",
        str(output),
    ]
    subprocess.run(command, check=True)
    print(f"生成 {output.relative_to(ROOT)}")
    print("  命令：" + " ".join(command))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", choices=("gcc", "clang"))
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()

    if args.clean:
        if GENERATED.exists():
            shutil.rmtree(GENERATED)
            print("已删除 generated/")
        return 0

    names = (args.compiler,) if args.compiler else ("gcc", "clang")
    available: list[tuple[str, str]] = []
    for name in names:
        executable = shutil.which(name)
        if executable:
            available.append((name, executable))

    if not available:
        parser.error("PATH 中没有找到 GCC 或 Clang")

    GENERATED.mkdir(exist_ok=True)
    for name, executable in available:
        print(f"{name}: {compiler_version(executable)}")
        for optimization in ("O0", "O2"):
            generate(executable, name, optimization)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
