#!/usr/bin/env python3
"""运行保存的 Linux 6.12.20 LKMM Litmus 测试并核对 Observation。"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent
REPOSITORY = ROOT.parents[3]
MODEL = REPOSITORY / "research" / "source_reading" / "linux" / "tools" / "memory-model"
TESTS = ROOT / "tests"
GENERATED = ROOT / "generated"
MANIFEST = ROOT / "manifest.json"
MODEL_FILES = (
    "linux-kernel.cfg",
    "linux-kernel.def",
    "linux-kernel.bell",
    "linux-kernel.cat",
    "lock.cat",
)


def load_manifest() -> dict:
    return json.loads(MANIFEST.read_text(encoding="utf-8"))


def validate(manifest: dict) -> list[str]:
    errors: list[str] = []
    for name in MODEL_FILES:
        if not (MODEL / name).is_file():
            errors.append(f"缺少模型文件：{MODEL / name}")

    allowed = {"Sometimes", "Never"}
    for entry in manifest.get("tests", []):
        if entry.get("expected") not in allowed:
            errors.append(f"非法预期结果：{entry}")
        path = TESTS / entry.get("file", "")
        if not path.is_file():
            errors.append(f"缺少测试文件：{path}")

    if not manifest.get("tests"):
        errors.append("manifest 没有测试")
    return errors


def run_test(herd7: str, entry: dict) -> None:
    test = TESTS / entry["file"]
    command = [herd7, "-conf", "linux-kernel.cfg", str(test.resolve())]
    result = subprocess.run(
        command,
        cwd=MODEL,
        check=True,
        capture_output=True,
        text=True,
    )
    GENERATED.mkdir(exist_ok=True)
    output = GENERATED / f"{test.stem}.txt"
    output.write_text(result.stdout, encoding="utf-8")

    match = re.search(r"^Observation\s+\S+\s+(Sometimes|Never)\b", result.stdout, re.MULTILINE)
    if not match:
        raise RuntimeError(f"无法解析 {test.name} 的 Observation，完整输出在 {output}")
    actual = match.group(1)
    expected = entry["expected"]
    print(f"{test.name}: {actual}（预期 {expected}）")
    if actual != expected:
        raise RuntimeError(f"{test.name} 结果不一致，完整输出在 {output}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--clean", action="store_true")
    parser.add_argument("--filter", default="")
    args = parser.parse_args()

    if args.clean:
        if GENERATED.exists():
            shutil.rmtree(GENERATED)
            print("已删除 generated/")
        return 0

    manifest = load_manifest()
    errors = validate(manifest)
    if errors:
        for error in errors:
            print("错误：" + error)
        return 1

    herd7 = shutil.which("herd7")
    print(f"模型基线：Linux {manifest['linux_model']}")
    print(f"模型目录：{MODEL}")
    if not herd7:
        print("herd7：未安装；静态结构有效，但当前环境不能运行模型")
        return 0 if args.check else 2

    version = subprocess.run(
        [herd7, "-version"],
        check=True,
        capture_output=True,
        text=True,
    )
    print("herd7：" + (version.stdout.strip() or version.stderr.strip()))
    if args.check:
        return 0

    selected = [
        entry for entry in manifest["tests"]
        if args.filter.lower() in entry["file"].lower()
    ]
    if not selected:
        parser.error("--filter 没有匹配任何测试")
    for entry in selected:
        run_test(herd7, entry)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
