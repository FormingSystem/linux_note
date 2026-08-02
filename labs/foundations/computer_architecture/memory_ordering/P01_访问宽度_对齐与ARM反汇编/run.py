#!/usr/bin/env python3
"""为 host 和 Cortex-A7 生成访问宽度实验汇编。"""

from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "src" / "access_width.c"
GENERATED = ROOT / "generated"


def emit(name: str, executable: str, options: list[str]) -> None:
    output = GENERATED / f"{name}.s"
    command = [
        executable,
        *options,
        "-S",
        "-fno-asynchronous-unwind-tables",
        "-fno-ident",
        str(SOURCE),
        "-o",
        str(output),
    ]
    subprocess.run(command, check=True)
    version = subprocess.run(
        [executable, "--version"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.splitlines()[0]
    print(f"{name}: {version}")
    print("  命令：" + " ".join(command))
    print(f"  输出：{output.relative_to(ROOT)}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", choices=("host", "arm"))
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()

    if args.clean:
        if GENERATED.exists():
            shutil.rmtree(GENERATED)
            print("已删除 generated/")
        return 0

    GENERATED.mkdir(exist_ok=True)
    ran = False

    if args.target in (None, "host"):
        host = shutil.which("gcc")
        if host:
            emit("host_gcc_O2", host, ["-O2"])
            ran = True
        elif args.target == "host":
            parser.error("PATH 中没有找到 gcc")

    if args.target in (None, "arm"):
        arm = shutil.which("arm-none-eabi-gcc")
        if arm:
            emit("cortex_a7_gcc_O2", arm, ["-O2", "-mcpu=cortex-a7", "-marm"])
            ran = True
        elif args.target == "arm":
            parser.error("PATH 中没有找到 arm-none-eabi-gcc")

    if not ran:
        parser.error("没有找到可用的 host GCC 或 ARM GCC")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
