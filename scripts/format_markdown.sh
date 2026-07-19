#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
用法：
  ./format.sh check headings [--summary] [Markdown文件或目录...]
  ./format.sh fix headings [--summary] [Markdown文件或目录...]

默认仅预览；不给路径时处理 Git 管理的全部 Markdown 文件。
标题序号：H1=第N章、H2=N.N、H3=N.N.N、H4=(N)、H5=N)、H6=a)；标题下划线统一写成 \_。
EOF
}

mode=preview
detail=true
paths=()
for argument in "$@"; do
    case "$argument" in
        --apply) mode=apply ;;
        --summary) detail=false ;;
        -h|--help) usage; exit 0 ;;
        --*) printf '未知参数：%s\n' "$argument" >&2; exit 2 ;;
        *) paths+=("$argument") ;;
    esac
done

for command_name in git python3; do
    command -v "$command_name" >/dev/null 2>&1 || {
        printf '缺少依赖：%s\n' "$command_name" >&2
        exit 127
    }
done

repo_root=$(git rev-parse --show-toplevel)
cd "$repo_root"
export PYTHONUTF8=1
export PYTHONIOENCODING=utf-8

python3 - "$mode" "$detail" "${paths[@]}" <<'PY'
from __future__ import annotations

import re
import subprocess
import sys
import unicodedata
from collections import Counter, defaultdict
from pathlib import Path

APPLY = sys.argv[1] == "apply"
DETAIL = sys.argv[2] == "true"
SELECTIONS = [item.replace("\\", "/").removeprefix("./").rstrip("/") for item in sys.argv[3:]]
ATX = re.compile(r"^(#{1,6})[ \t]+(.+?)[ \t]*$", re.MULTILINE)
FENCE = re.compile(r"^[ \t]*(`{3,}|~{3,})")
SETEXT = re.compile(r"^[ \t]*(=+|-+)[ \t]*$")
CHINESE_PUNCTUATION = re.compile(r"[：，。、“”‘’？！；、·—]")


def unescape_title_underscores(value: str) -> str:
    """将标题源码中的一个或多个反斜杠加下划线还原为规范文本。"""
    return re.sub(r"\\+_", "_", value)


def escape_title_underscores(value: str) -> str:
    """转义标题中的下划线，避免不同 Markdown 渲染器将其解析为强调。"""
    return value.replace("_", r"\_")


def alpha_number(number: int) -> str:
    result = ""
    while number:
        number, remainder = divmod(number - 1, 26)
        result = chr(ord("a") + remainder) + result
    return result


def strip_old_number(title: str, level: int) -> str:
    value = unescape_title_underscores(title.strip().rstrip("#").rstrip())
    value = value.replace("`", "").replace("**", "").replace("__", "")
    value = value.lstrip()
    while value and unicodedata.category(value[0]) in {"So", "Cs"}:
        value = value[1:].lstrip()
    patterns = [
        r"^P\d+[_\-\s]*",
        r"^第[ _]*\d+[ _]*(?:章|部分|篇)[_\-\s：:、]*",
        r"^\d+(?:\.\d+)+[.)、]?[_\-\s]*",
        r"^\(\d+\)[_\-\s]*",
        r"^\d+\)[_\-\s]*",
        r"^[A-Za-z]+\)[_\-\s]*",
        r"^[一二三四五六七八九十百]+[、.)）][_\-\s]*",
        r"^[①②③④⑤⑥⑦⑧⑨⑩⑪⑫⑬⑭⑮⑯⑰⑱⑲⑳][_\-\s]*",
        r"^\d+[.、][_\-\s]*",
    ]
    for _ in range(10):
        for pattern in patterns:
            replaced = re.sub(pattern, "", value, count=1)
            if replaced != value:
                value = replaced
                break
        else:
            break
    return value


def normalize_text(value: str) -> str:
    value = unescape_title_underscores(value)
    value = value.replace("`", "").replace("**", "").replace("__", "")
    value = "".join(character for character in value if unicodedata.category(character) not in {"So", "Cs"})
    value = re.sub(r"\s+-\s+", "_", value)
    value = re.sub(r"\s+", "_", value)
    value = value.replace("↔", "_to_").replace("→", "_to_").replace("←", "_to_")
    value = value.replace("×", "_x_")
    value = value.translate(str.maketrans({"（": "(", "）": ")", "【": "(", "】": ")"}))
    value = value.replace("《", "_").replace("》", "_")
    value = CHINESE_PUNCTUATION.sub("_", value)
    value = re.sub(r"_+", "_", value)
    value = re.sub(r"_([.)])", r"\1", value)
    return value.strip("_") or "未命名标题"


def sequence(level: int, counters: list[int]) -> str:
    if level == 1:
        return f"第{counters[1]}章"
    if level == 2:
        return f"{counters[1]}.{counters[2]}"
    if level == 3:
        return f"{counters[1]}.{counters[2]}.{counters[3]}"
    if level == 4:
        return f"({counters[4]})"
    if level == 5:
        return f"{counters[5]})"
    return f"{alpha_number(counters[6])})"


def headings_outside_fences(lines: list[str]) -> list[tuple[int, int, str]]:
    result: list[tuple[int, int, str]] = []
    in_fence = False
    fence_marker = ""
    for index, line in enumerate(lines):
        stripped = line.rstrip("\r\n")
        fence = FENCE.match(stripped)
        if fence:
            marker = fence.group(1)[0]
            if not in_fence:
                in_fence, fence_marker = True, marker
            elif marker == fence_marker:
                in_fence, fence_marker = False, ""
            continue
        if in_fence:
            continue
        match = ATX.match(stripped)
        if match:
            result.append((index, len(match.group(1)), match.group(2).rstrip("#").rstrip()))
    return result


raw = subprocess.run(
    ["git", "-c", "core.quotePath=false", "ls-files", "-z", "-c", "-o", "--exclude-standard"],
    check=True,
    stdout=subprocess.PIPE,
).stdout
tracked = [item.decode("utf-8") for item in raw.split(b"\0") if item]
markdown_files = [
    path for path in tracked
    if Path(path).exists() and Path(path).suffix.casefold() in {".md", ".markdown"}
]
selected = markdown_files
if SELECTIONS:
    selected = [
        path for path in markdown_files
        if any(path == scope or path.startswith(scope + "/") for scope in SELECTIONS)
    ]
    missing = [scope for scope in SELECTIONS if not any(path == scope or path.startswith(scope + "/") for path in selected)]
    if missing:
        raise SystemExit("没有找到受 Git 管理的 Markdown：" + ", ".join(missing))

changes: dict[str, list[tuple[int, str, str]]] = defaultdict(list)
rewritten: dict[str, str] = {}
file_anchor_maps: dict[str, dict[str, str]] = {}

for file_name in selected:
    with open(file_name, "r", encoding="utf-8", newline="") as stream:
        lines = stream.readlines()

    # 先将 Setext H1/H2 转换为统一的 ATX 写法，同时跳过 front matter 和代码围栏。
    converted: list[str] = []
    index = 0
    in_fence = False
    fence_marker = ""
    in_front_matter = bool(lines and lines[0].strip() == "---")
    while index < len(lines):
        current = lines[index].rstrip("\r\n")
        if in_front_matter:
            converted.append(lines[index])
            if index > 0 and current.strip() in {"---", "..."}:
                in_front_matter = False
            index += 1
            continue
        fence = FENCE.match(current)
        if fence:
            marker = fence.group(1)[0]
            if not in_fence:
                in_fence, fence_marker = True, marker
            elif marker == fence_marker:
                in_fence, fence_marker = False, ""
            converted.append(lines[index])
            index += 1
            continue
        if not in_fence and index + 1 < len(lines) and lines[index].strip() and SETEXT.match(lines[index + 1].rstrip("\r\n")):
            underline = SETEXT.match(lines[index + 1].rstrip("\r\n"))
            assert underline is not None
            level = 1 if underline.group(1).startswith("=") else 2
            newline = "\r\n" if lines[index].endswith("\r\n") else "\n"
            converted.append("#" * level + " " + lines[index].strip() + newline)
            index += 2
            continue
        converted.append(lines[index])
        index += 1
    lines = converted

    # PXX_NAME.md 表示书籍的第 XX 章。若正文没有独立章标题，则补充 H1，
    # 并把原有标题整体下移一级，使原来的 5.1 成为 H2，而不是新的 H1。
    chapter_match = re.match(r"^P(\d+)_([^/]+)$", Path(file_name).stem)
    chapter_number = int(chapter_match.group(1)) if chapter_match else None
    chapter_title = chapter_match.group(2) if chapter_match else ""
    if chapter_number is not None:
        existing_headings = headings_outside_fences(lines)
        h1_items = [item for item in existing_headings if item[1] == 1]
        has_document_h1 = (
            len(h1_items) == 1
            and normalize_text(strip_old_number(h1_items[0][2], 1)).casefold()
            == normalize_text(chapter_title).casefold()
        )
        if not has_document_h1:
            for heading_index, heading_level, heading_title in reversed(existing_headings):
                new_level = min(heading_level + 1, 6)
                original_line = lines[heading_index]
                newline = "\r\n" if original_line.endswith("\r\n") else "\n" if original_line.endswith("\n") else ""
                lines[heading_index] = "#" * new_level + " " + heading_title + newline

            insert_at = 0
            if lines and lines[0].strip() == "---":
                for front_index in range(1, len(lines)):
                    if lines[front_index].strip() in {"---", "..."}:
                        insert_at = front_index + 1
                        break
            newline = "\r\n" if any(line.endswith("\r\n") for line in lines[:20]) else "\n"
            lines[insert_at:insert_at] = [f"# {chapter_title}{newline}", newline]

    counters = [0] * 7
    in_fence = False
    fence_marker = ""
    anchors: list[tuple[str, str]] = []

    for index, line in enumerate(lines):
        stripped = line.rstrip("\r\n")
        fence = FENCE.match(stripped)
        if fence:
            marker = fence.group(1)[0]
            if not in_fence:
                in_fence, fence_marker = True, marker
            elif marker == fence_marker:
                in_fence, fence_marker = False, ""
            continue
        if in_fence:
            continue
        match = ATX.match(stripped)
        if not match:
            continue

        level = len(match.group(1))
        if level == 1:
            if chapter_number is not None:
                counters[1] = chapter_number
            else:
                counters[1] += 1
        else:
            for parent in range(1, level):
                if counters[parent] == 0:
                    counters[parent] = 1
            counters[level] += 1
        for child in range(level + 1, 7):
            counters[child] = 0

        old_title = match.group(2).rstrip("#").rstrip()
        if level == 1 and chapter_number is not None:
            old_title = match.group(2).rstrip("#").rstrip()
            title_source = chapter_title
        else:
            title_source = strip_old_number(old_title, level)
        title_text = normalize_text(title_source)
        new_anchor = sequence(level, counters) + "_" + title_text
        new_title = escape_title_underscores(new_anchor)
        old_anchor = unescape_title_underscores(old_title)
        anchors.append((old_anchor, new_anchor))
        if old_title == new_title:
            continue
        newline = "\r\n" if line.endswith("\r\n") else "\n" if line.endswith("\n") else ""
        lines[index] = f"{'#' * level} {new_title}{newline}"
        changes[file_name].append((index + 1, old_title, new_title))

    rewritten[file_name] = "".join(lines)
    old_counts = Counter(old for old, _ in anchors)
    file_anchor_maps[file_name] = {old: new for old, new in anchors if old_counts[old] == 1 and old != new}

change_count = sum(len(items) for items in changes.values())
print(f"待格式化标题：{change_count}（Markdown文件：{len(selected)}）")
if DETAIL:
    for file_name, items in changes.items():
        for line_number, old_title, new_title in items:
            print(f"{file_name}:{line_number}: {old_title} -> {new_title}")

if not APPLY:
    raise SystemExit(1 if change_count else 0)

# 先写标题，再维护同文件锚点；跨文件中无歧义的同名锚点也一并更新。
global_targets: dict[str, set[str]] = defaultdict(set)
for anchor_map in file_anchor_maps.values():
    for old, new in anchor_map.items():
        global_targets[old].add(new)
global_unambiguous = {old: next(iter(targets)) for old, targets in global_targets.items() if len(targets) == 1}

for file_name in markdown_files:
    if file_name in rewritten:
        content = rewritten[file_name]
    else:
        with open(file_name, "r", encoding="utf-8", newline="") as stream:
            content = stream.read()
    original = content
    replacements = dict(global_unambiguous)
    replacements.update(file_anchor_maps.get(file_name, {}))
    for old_title, new_title in replacements.items():
        content = content.replace("#" + old_title, "#" + new_title)
        content = content.replace("#" + old_title.replace(" ", "%20"), "#" + new_title)
    content = re.sub(r"[ \t]+(?=\r?$)", "", content, flags=re.MULTILINE)
    if content != original or file_name in changes:
        with open(file_name, "w", encoding="utf-8", newline="") as stream:
            stream.write(content)

print(f"已格式化标题：{change_count}")
PY

if [[ $mode == apply ]]; then
    git diff --check
    printf 'Markdown 标题规范化完成；请检查 git diff 后再提交。\n'
fi
