#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
用法：
  ./format.sh           # 仅预览
  ./format.sh --apply   # 执行重命名并更新引用
  ./format.sh --help

依赖：bash、git、Python 3（MSYS2 可安装 mingw-w64-ucrt-x86_64-python）
EOF
}

mode=preview
case "${1:-}" in
    '') ;;
    --apply) mode=apply ;;
    -h|--help) usage; exit 0 ;;
    *) printf '未知参数：%s\n' "$1" >&2; usage >&2; exit 2 ;;
esac

for command_name in git python3; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        printf '缺少依赖：%s\n' "$command_name" >&2
        if [[ $command_name == python3 ]]; then
            printf 'MSYS2 安装命令：pacman -S --needed mingw-w64-ucrt-x86_64-python\n' >&2
        fi
        exit 127
    fi
done

repo_root=$(git rev-parse --show-toplevel)
cd "$repo_root"

export PYTHONUTF8=1
export PYTHONIOENCODING=utf-8
python3 - "$mode" <<'PY'
from __future__ import annotations

import os
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

APPLY = sys.argv[1] == "apply"
TEXT_EXTENSIONS = {
    ".md", ".markdown", ".json", ".mm", ".txt", ".rst",
    ".c", ".h", ".css", ".js", ".yml", ".yaml",
}
CHINESE_PUNCTUATION = re.compile(r"[：，。、“”‘’？！；、·—]")
EXTERNAL_TARGET = re.compile(r"^(?:[A-Za-z][A-Za-z0-9+.-]*:|#)")


def run(*args: str, capture: bool = False) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        args,
        check=True,
        stdout=subprocess.PIPE if capture else None,
    )


def normalize_segment(segment: str) -> str:
    if not segment:
        return segment

    value = re.sub(
        r"^P0*(\d{1,2})(?=[_\-\s]|$)",
        lambda match: f"P{int(match.group(1)):02d}",
        segment,
    )
    value = re.sub(r"\s+-\s+", "_", value)
    value = re.sub(r"^(P\d+)-", r"\1_", value)
    value = re.sub(r"\s+", "_", value)
    value = re.sub(r"^第_?(\d+)_?章_?", lambda match: f"第{int(match.group(1))}章_", value)
    value = value.replace("↔", "_to_").replace("→", "_to_").replace("←", "_to_")
    value = value.replace("×", "_x_")
    value = value.translate(str.maketrans({"（": "(", "）": ")", "【": "(", "】": ")"}))
    value = value.replace("《", "_").replace("》", "_")
    value = CHINESE_PUNCTUATION.sub("_", value).replace("`", "_")
    value = re.sub(r"_+", "_", value)
    value = re.sub(r"_([.)])", r"\1", value)
    return value.strip("_")


def normalize_path(path: str) -> str:
    return "/".join(normalize_segment(part) for part in path.replace("\\", "/").split("/"))


def normalize_link_target(target: str) -> str:
    if EXTERNAL_TARGET.match(target):
        return target

    path_part, marker, fragment = target.partition("#")
    prefix = ""
    if path_part.startswith("./"):
        prefix, path_part = "./", path_part[2:]
    path_part = path_part.replace("%20", "_")
    normalized = prefix + normalize_path(path_part)
    return normalized + (marker + fragment if marker else "")


tracked_raw = run("git", "-c", "core.quotePath=false", "ls-files", "-z", capture=True).stdout
tracked = [item.decode("utf-8") for item in tracked_raw.split(b"\0") if item]
mapping = {old: normalize_path(old) for old in tracked}
mapping = {old: new for old, new in mapping.items() if old != new}

targets: dict[str, list[str]] = defaultdict(list)
for old, new in mapping.items():
    targets[new.casefold()].append(old)

collisions = {key: olds for key, olds in targets.items() if len(olds) > 1}
tracked_casefold = {path.casefold() for path in tracked}
for old, new in mapping.items():
    if new.casefold() in tracked_casefold and new.casefold() not in {item.casefold() for item in mapping}:
        collisions[new.casefold()].append(old)

if collisions:
    print("命名规则产生路径冲突：", file=sys.stderr)
    for target, sources in collisions.items():
        print(f"  {target} <= {', '.join(sources)}", file=sys.stderr)
    raise SystemExit(1)

print(f"待重命名文件：{len(mapping)}")
if not APPLY:
    for old, new in mapping.items():
        print(f"{old} -> {new}")
    raise SystemExit(0)

updated: dict[str, str] = {}
markdown_link = re.compile(r"(?P<prefix>!?\[[^\]]*\]\()(?P<target>[^)]+)(?P<suffix>\))")
wiki_link = re.compile(r"(?P<prefix>!?\[\[)(?P<target>[^\]|]+)(?P<suffix>(?:\|[^\]]*)?\]\])")
html_link = re.compile(r"(?P<prefix>\b(?:src|href)=[\"'])(?P<target>[^\"']+)(?P<suffix>[\"'])")

for old in tracked:
    if Path(old).suffix.casefold() not in TEXT_EXTENSIONS:
        continue
    try:
        content = Path(old).read_text(encoding="utf-8")
    except UnicodeDecodeError:
        continue
    original = content

    if Path(old).suffix.casefold() in {".md", ".markdown"}:
        def replace_target(match: re.Match[str]) -> str:
            return match["prefix"] + normalize_link_target(match["target"]) + match["suffix"]

        content = markdown_link.sub(replace_target, content)
        content = wiki_link.sub(replace_target, content)
        content = html_link.sub(replace_target, content)

    for source, target in mapping.items():
        content = content.replace(source, target)
        content = content.replace(source.replace("/", "\\"), target.replace("/", "\\"))
    if content != original:
        updated[old] = content

for old, new in sorted(mapping.items(), key=lambda item: item[0].count("/"), reverse=True):
    Path(new).parent.mkdir(parents=True, exist_ok=True)
    run("git", "mv", "--", old, new)

for old, content in updated.items():
    destination = mapping.get(old, old)
    Path(destination).write_text(content, encoding="utf-8", newline="")

print(f"已重命名文件：{len(mapping)}")
PY

if [[ $mode == apply ]]; then
    git diff --check
    printf '规范化完成；请检查 git diff 后再提交。\n'
fi
