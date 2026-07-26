#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
用法：
  ./format.sh check links [--summary] [Markdown文件或目录...]
  ./format.sh fix links [--summary] [Markdown文件或目录...]

默认仅扫描。不给路径时扫描全仓；指定文件或目录时只扫描对应 Markdown。
自动修复仅用于唯一确定的目标；歧义链接和缺失资源只报告、不改写。
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

import os
import posixpath
import re
import subprocess
import sys
from difflib import get_close_matches
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from urllib.parse import unquote, urlsplit

APPLY = sys.argv[1] == "apply"
DETAIL = sys.argv[2] == "true"
SELECTIONS = [item.replace("\\", "/").removeprefix("./").rstrip("/") for item in sys.argv[3:]]
EXTERNAL = re.compile(r"^(?:[A-Za-z][A-Za-z0-9+.-]*:|//|mailto:|data:)")
WINDOWS_ABSOLUTE = re.compile(r"^[A-Za-z]:[\\/]")
POSIX_ABSOLUTE = re.compile(r"^/(?!/)")
LOCAL_URI = re.compile(r"^(?:file:|vscode-resource:|file\+\.vscode-resource)", re.IGNORECASE)
HTML_LINK = re.compile(r"(?P<prefix>\b(?:src|href)=[\"'])(?P<target>[^\"']+)(?P<suffix>[\"'])", re.IGNORECASE)
WIKI_LINK = re.compile(r"(?P<prefix>!?\[\[)(?P<body>[^\]]+)(?P<suffix>\]\])")


@dataclass
class Link:
    start: int
    end: int
    target_start: int
    target_end: int
    target: str
    kind: str


def git_output(*arguments: str) -> bytes:
    return subprocess.run(["git", *arguments], check=True, stdout=subprocess.PIPE).stdout


def normalize_repo_path(value: str) -> str:
    normalized = posixpath.normpath(value.replace("\\", "/"))
    return "" if normalized == "." else normalized.removeprefix("./")


def normalize_link_path(value: str) -> str:
    """区分Markdown字符转义与Windows目录分隔符。"""
    unescaped = re.sub(r"\\([_*.()\[\]#])", r"\1", unquote(value))
    return unescaped.replace("\\", "/")


def split_fragment(target: str) -> tuple[str, str]:
    path, marker, fragment = target.partition("#")
    return path, marker + fragment if marker else ""


def markdown_links(content: str) -> list[Link]:
    links: list[Link] = []
    index = 0
    while index < len(content) - 1:
        open_paren = content.find("](", index)
        if open_paren < 0:
            break
        cursor = open_paren + 2
        depth = 1
        escaped = False
        while cursor < len(content):
            character = content[cursor]
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == "(":
                depth += 1
            elif character == ")":
                depth -= 1
                if depth == 0:
                    raw = content[open_paren + 2:cursor]
                    if raw.startswith("<") and ">" in raw:
                        close_angle = raw.find(">")
                        target = raw[1:close_angle]
                        offset = 1
                    else:
                        title_match = re.match(r"([^\s]+)(?:\s+[\"'].*)?$", raw, re.DOTALL)
                        if not title_match:
                            break
                        target = title_match.group(1)
                        offset = title_match.start(1)
                    links.append(Link(open_paren, cursor + 1, open_paren + 2 + offset, open_paren + 2 + offset + len(target), target, "markdown"))
                    break
            cursor += 1
        index = max(cursor + 1, open_paren + 2)
    return links


def collect_links(content: str) -> list[Link]:
    links = markdown_links(content)
    for match in WIKI_LINK.finditer(content):
        body = match.group("body")
        target = body.split("|", 1)[0]
        start = match.start("body")
        links.append(Link(match.start(), match.end(), start, start + len(target), target, "wiki"))
    for match in HTML_LINK.finditer(content):
        links.append(Link(match.start(), match.end(), match.start("target"), match.end("target"), match.group("target"), "html"))
    ranges: list[tuple[int, int]] = []
    offset = 0
    in_fence = False
    fence_marker = ""
    for line in content.splitlines(keepends=True):
        fence = re.match(r"^[ \t]*(`{3,}|~{3,})", line)
        if fence:
            marker = fence.group(1)[0]
            if not in_fence:
                in_fence, fence_marker = True, marker
                ranges.append((offset, offset + len(line)))
            elif marker == fence_marker:
                ranges.append((offset, offset + len(line)))
                in_fence, fence_marker = False, ""
            offset += len(line)
            continue
        if in_fence:
            ranges.append((offset, offset + len(line)))
        else:
            for inline in re.finditer(r"(`+)(.+?)\1", line):
                ranges.append((offset + inline.start(), offset + inline.end()))
        offset += len(line)

    def is_code(position: int) -> bool:
        return any(start <= position < end for start, end in ranges)

    return sorted((link for link in links if not is_code(link.start)), key=lambda item: item.start)


def rename_map() -> dict[str, str]:
    raw = git_output("diff", "--name-status", "-z", "-M", "HEAD")
    fields = [field.decode("utf-8") for field in raw.split(b"\0") if field]
    result: dict[str, str] = {}
    index = 0
    while index < len(fields):
        status = fields[index]
        index += 1
        if status.startswith("R") and index + 1 < len(fields):
            old, new = fields[index], fields[index + 1]
            result[normalize_repo_path(old)] = normalize_repo_path(new)
            index += 2
        elif index < len(fields):
            index += 1
    return result


tracked_raw = git_output("-c", "core.quotePath=false", "ls-files", "-z", "-c", "-o", "--exclude-standard")
tracked = [item.decode("utf-8") for item in tracked_raw.split(b"\0") if item]
existing_files = {normalize_repo_path(path) for path in tracked if Path(path).exists()}
existing_directories = {
    str(parent)
    for path in existing_files
    for parent in PurePosixPath(path).parents
    if str(parent) != "."
}
existing = existing_files | existing_directories
markdown_files = sorted(path for path in existing_files if Path(path).suffix.casefold() in {".md", ".markdown"})

selected = markdown_files
if SELECTIONS:
    selected = [
        path for path in markdown_files
        if any(path == scope or path.startswith(scope + "/") for scope in SELECTIONS)
    ]
    missing = [scope for scope in SELECTIONS if not any(path == scope or path.startswith(scope + "/") for path in selected)]
    if missing:
        raise SystemExit(
            "没有找到 Markdown：" + ", ".join(missing)
            + "\n提示：Bash 中请使用正斜杠路径，或给含反斜杠的完整路径加引号。"
        )

renames = rename_map()
by_name: dict[str, list[str]] = defaultdict(list)
by_stem: dict[str, list[str]] = defaultdict(list)
for path in existing:
    pure = PurePosixPath(path)
    by_name[pure.name.casefold()].append(path)
    by_stem[pure.stem.casefold()].append(path)


def old_target_path(source: str, link_path: str, kind: str) -> str:
    decoded = normalize_link_path(link_path)
    if decoded.startswith("/"):
        return normalize_repo_path(decoded.lstrip("/"))
    if kind == "wiki" and not decoded.startswith("."):
        return normalize_repo_path(decoded)
    return normalize_repo_path(posixpath.join(posixpath.dirname(source), decoded))


def windows_repo_path(link_path: str) -> str | None:
    """将当前仓库内的Windows绝对路径转换为仓库相对路径。"""
    decoded = normalize_link_path(link_path)
    if not WINDOWS_ABSOLUTE.match(decoded):
        return None
    root = Path.cwd().resolve().as_posix().rstrip("/")
    prefix = root + "/"
    if not decoded.casefold().startswith(prefix.casefold()):
        return None
    return normalize_repo_path(decoded[len(prefix):])


def local_uri_file_path(link_path: str) -> str:
    """从file或vscode-resource URI中提取本地文件路径。"""
    parsed = urlsplit(link_path)
    decoded = unquote(parsed.path).replace("\\", "/")
    if parsed.netloc and parsed.scheme.casefold() == "file":
        decoded = f"//{parsed.netloc}{decoded}"
    if re.match(r"^/[A-Za-z]:/", decoded):
        decoded = decoded[1:]
    return decoded


def candidate_target(source: str, link_path: str, kind: str) -> tuple[str | None, str]:
    absolute_path = False
    if LOCAL_URI.match(link_path):
        local_path = local_uri_file_path(link_path)
        candidate, reason = candidate_target(source, local_path, kind)
        if candidate is not None:
            return candidate, "absolute"
        return None, reason if reason in {"ambiguous", "local_absolute"} else "local_absolute"
    if WINDOWS_ABSOLUTE.match(link_path):
        repository_path = windows_repo_path(link_path)
        if repository_path is None:
            decoded = unquote(link_path).replace("\\", "/")
            name = PurePosixPath(decoded).name
            pool = by_name.get(name.casefold(), [])
            if len(pool) == 1:
                return pool[0], "absolute"
            if len(pool) > 1:
                return None, "ambiguous"
            return None, "local_absolute"
        old_path = repository_path
        absolute_path = True
    else:
        old_path = old_target_path(source, link_path, kind)
        absolute_path = bool(POSIX_ABSOLUTE.match(link_path))
    variants = [old_path]
    if not PurePosixPath(old_path).suffix:
        variants.extend(old_path + extension for extension in (".md", ".markdown"))

    name = PurePosixPath(old_path).name
    extensionless = not PurePosixPath(name).suffix
    if kind == "wiki" and "/" not in link_path.replace("\\", "/"):
        short_pool = by_stem.get(name.casefold(), []) if extensionless else by_name.get(name.casefold(), [])
        if len(short_pool) == 1:
            return None, "valid"

    if any(variant in existing for variant in variants):
        if absolute_path:
            return next(variant for variant in variants if variant in existing), "absolute"
        return None, "valid"
    for variant in variants:
        if variant in renames:
            return renames[variant], "rename"

    pool = by_name.get(name.casefold(), [])
    if extensionless:
        pool = by_stem.get(name.casefold(), [])
    if len(pool) == 1:
        return pool[0], "unique"
    if len(pool) > 1:
        return None, "ambiguous"
    return None, "missing"


def render_target(source: str, target: str, original_path: str, fragment: str, kind: str) -> str:
    if kind == "wiki":
        rendered = target
        if not PurePosixPath(original_path).suffix and PurePosixPath(rendered).suffix.casefold() in {".md", ".markdown"}:
            rendered = str(PurePosixPath(rendered).with_suffix(""))
    else:
        rendered = posixpath.relpath(target, posixpath.dirname(source) or ".")
        if original_path.startswith("./") and not rendered.startswith("."):
            rendered = "./" + rendered
    return rendered + fragment


statistics = Counter(valid=0, external=0, anchor=0, updated=0, ambiguous=0, missing=0, local_absolute=0, obsidian=0)
messages: list[str] = []


def link_location(content: str, position: int) -> tuple[int, int]:
    line = content.count("\n", 0, position) + 1
    line_start = content.rfind("\n", 0, position) + 1
    return line, position - line_start + 1


def candidate_hints(link_path: str, limit: int = 6) -> list[str]:
    decoded = normalize_link_path(link_path)
    name = PurePosixPath(decoded).name.casefold()
    direct = sorted(set(by_name.get(name, []) + by_stem.get(PurePosixPath(name).stem, [])))
    if direct:
        return direct[:limit]
    close_names = get_close_matches(name, list(by_name), n=limit, cutoff=0.55)
    return [path for close_name in close_names for path in by_name[close_name]][:limit]

for source in selected:
    with open(source, "r", encoding="utf-8", newline="") as stream:
        content = stream.read()
    replacements: list[tuple[int, int, str]] = []
    for link in collect_links(content):
        target = link.target.strip()
        line, column = link_location(content, link.target_start)
        location = f"{source}:{line}:{column}"
        if not target:
            statistics["missing"] += 1
            messages.append(f"MISSING {location}: 空链接（类型={link.kind}）")
            continue
        if EXTERNAL.match(target) and not WINDOWS_ABSOLUTE.match(target) and not LOCAL_URI.match(target):
            statistics["external"] += 1
            continue
        path_part, fragment = split_fragment(target)
        if not path_part:
            statistics["anchor"] += 1
            continue
        candidate, reason = candidate_target(source, path_part, link.kind)
        if reason == "valid":
            statistics["valid"] += 1
            continue
        if candidate is None:
            statistics[reason] += 1
            hints = candidate_hints(path_part)
            suffix = "；候选=" + " | ".join(hints) if hints else "；候选=无"
            messages.append(f"{reason.upper()} {location}: {target}（类型={link.kind}）{suffix}")
            continue
        rendered = render_target(source, candidate, path_part, fragment, link.kind)
        if rendered != target:
            replacements.append((link.target_start, link.target_end, rendered))
            statistics["updated"] += 1
            messages.append(f"UPDATE {location}: {target} -> {rendered}（依据={reason}）")

    if APPLY and replacements:
        for start, end, rendered in sorted(replacements, reverse=True):
            content = content[:start] + rendered + content[end:]
        with open(source, "w", encoding="utf-8", newline="") as stream:
            stream.write(content)

if renames:
    auxiliary = [
        path for path in tracked
        if (path.startswith(".obsidian/") and Path(path).suffix.casefold() == ".json")
        or Path(path).suffix.casefold() in {".canvas", ".base"}
    ]
    for file_name in auxiliary:
        try:
            with open(file_name, "r", encoding="utf-8", newline="") as stream:
                content = stream.read()
        except (OSError, UnicodeDecodeError):
            continue
        original = content
        for old, new in renames.items():
            # 只替换独立的仓库路径。新路径可能包含旧路径（例如
            # assets/images/... 包含 images/...），边界限制保证重复运行幂等。
            content = re.sub(
                rf"(?<![A-Za-z0-9_.\\/\-]){re.escape(old)}",
                lambda _match, value=new: value,
                content,
            )
            old_windows = old.replace("/", "\\")
            new_windows = new.replace("/", "\\")
            content = re.sub(
                rf"(?<![A-Za-z0-9_.\\/\-]){re.escape(old_windows)}",
                lambda _match, value=new_windows: value,
                content,
            )
        if content != original:
            statistics["obsidian"] += 1
            messages.append(f"OBSIDIAN {file_name}: 更新已重命名路径")
            if APPLY:
                with open(file_name, "w", encoding="utf-8", newline="") as stream:
                    stream.write(content)

print(
    "链接扫描："
    f"Markdown={len(selected)} "
    f"有效={statistics['valid']} 外部={statistics['external']} 纯锚点={statistics['anchor']} "
    f"可更新={statistics['updated']} 歧义={statistics['ambiguous']} 缺失={statistics['missing']} "
    f"本地绝对路径={statistics['local_absolute']} "
    f"Obsidian配置={statistics['obsidian']}"
)
for message in messages:
    if DETAIL or message.startswith(("MISSING ", "AMBIGUOUS ", "LOCAL_ABSOLUTE ")):
        print(message)
if APPLY:
    print(f"已更新链接：{statistics['updated']}")
    unresolved = statistics["ambiguous"] + statistics["missing"] + statistics["local_absolute"]
    if unresolved:
        raise SystemExit(f"仍有未解决链接：{unresolved}")
else:
    problems = (
        statistics["updated"] + statistics["ambiguous"] + statistics["missing"]
        + statistics["local_absolute"] + statistics["obsidian"]
    )
    raise SystemExit(1 if problems else 0)
PY

if [[ $mode == apply ]]; then
    git diff --check
    printf '链接更新完成；请检查缺失与歧义报告后再提交。\n'
fi
