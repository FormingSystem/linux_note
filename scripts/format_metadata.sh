#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
用法：
  ./format.sh check metadata [--summary] [Markdown文件或目录...]
  ./format.sh fix metadata [--summary] [Markdown文件或目录...]

检查或补齐 Markdown 的 id、title、kind、status 和 domains。
已有 id 不因路径、标题或出版顺序变化而重算；本命令不修改正文标题。
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
export PYTHONUTF8=1 PYTHONIOENCODING=utf-8

python3 - "$mode" "$detail" "${paths[@]}" <<'PY'
from __future__ import annotations

import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

APPLY = sys.argv[1] == "apply"
DETAIL = sys.argv[2] == "true"
SELECTIONS = [item.replace("\\", "/").removeprefix("./").rstrip("/") for item in sys.argv[3:]]
VALID_KINDS = {"concept", "mechanism", "subsystem", "interface", "engineering", "platform", "lab", "project", "source", "investigation", "reference", "track", "publication"}
VALID_STATUS = {"draft", "evolving", "maintained", "archived"}


def git_markdown() -> list[str]:
    raw = subprocess.run(
        ["git", "-c", "core.quotePath=false", "ls-files", "-z", "-c", "-o", "--exclude-standard"],
        check=True,
        stdout=subprocess.PIPE,
    ).stdout
    return sorted(
        path for path in (item.decode("utf-8") for item in raw.split(b"\0") if item)
        if Path(path).exists() and Path(path).suffix.casefold() in {".md", ".markdown"}
    )


def classification(path: str) -> tuple[str, list[str]]:
    if path.startswith("atlas/"):
        return "track", ["navigation"]
    if path.startswith("knowledge/foundations/"):
        return "concept", ["foundations"]
    if path.startswith("knowledge/linux/"):
        return "mechanism", ["linux", "kernel"]
    if path.startswith("knowledge/kernel_subsystems/"):
        return "subsystem", ["linux", "kernel"]
    if path.startswith("knowledge/driver_model/"):
        return "subsystem", ["linux", "kernel", "driver"]
    if path.startswith("knowledge/system_software/"):
        return "engineering", ["linux", "system"]
    if path.startswith("engineering/"):
        return "engineering", ["engineering"]
    if path.startswith("platforms/"):
        return "platform", ["linux", "platform"]
    if path.startswith("labs/"):
        return "lab", ["linux", "lab"]
    if path.startswith("projects/"):
        return "project", ["project"]
    if path.startswith("research/source_reading/"):
        return "source", ["linux", "source"]
    if path.startswith("research/"):
        return "investigation", ["research"]
    if path.startswith("publications/"):
        return "publication", ["publication"]
    if path.startswith("governance/"):
        return "reference", ["governance"]
    if path.startswith("tools/"):
        return "reference", ["tools"]
    if path.startswith("reference/"):
        return "reference", ["reference"]
    return "reference", ["repository"]


def semantic_id(path: str) -> str:
    special = {"README.md": "repository.readme", "AGENTS.md": "repository.agents"}
    if path in special:
        return special[path]
    parts = []
    for raw in Path(path).with_suffix("").parts:
        # PXX 是仓库正式阅读顺序的一部分，同名章节用它区分稳定 ID。
        value = raw
        value = re.sub(r"[^\w.\-]+", "_", value, flags=re.UNICODE)
        value = re.sub(r"_+", "_", value).strip("_.-").casefold()
        if value:
            parts.append(value)
    return ".".join(parts)


def document_title(content: str, path: str) -> str:
    body = content
    front = re.match(r"^---\r?\n.*?\r?\n---\r?\n", body, re.DOTALL)
    if front:
        body = body[front.end():]
    match = re.search(r"^#\s+(.+?)\s*$", body, re.MULTILINE)
    title = match.group(1) if match else Path(path).stem
    title = re.sub(r"^第\d+章_?", "", title)
    return title.replace("_", " ").strip()


def front_matter(content: str) -> tuple[dict[str, object], int]:
    match = re.match(r"^---\r?\n(.*?)\r?\n---\r?\n", content, re.DOTALL)
    if not match:
        return {}, 0
    metadata: dict[str, object] = {}
    current_list: str | None = None
    for line in match.group(1).splitlines():
        item = re.match(r"^([A-Za-z_][A-Za-z0-9_]*):\s*(.*?)\s*$", line)
        if item:
            key, value = item.groups()
            metadata[key] = value.strip('"\'') if value else []
            current_list = key if not value else None
            continue
        list_item = re.match(r"^\s+-\s+(.+?)\s*$", line)
        if list_item and current_list and isinstance(metadata[current_list], list):
            metadata[current_list].append(list_item.group(1).strip('"\''))
    return metadata, match.end()


markdown = git_markdown()
selected = markdown
if SELECTIONS:
    selected = [path for path in markdown if any(path == scope or path.startswith(scope + "/") for scope in SELECTIONS)]
    missing = [scope for scope in SELECTIONS if not any(path == scope or path.startswith(scope + "/") for path in selected)]
    if missing:
        raise SystemExit(
            "没有找到 Markdown：" + ", ".join(missing)
            + "\n提示：Bash 中请使用正斜杠路径，或给含反斜杠的完整路径加引号。"
        )

records: dict[str, dict[str, object]] = {}
contents: dict[str, tuple[str, int]] = {}
for path in markdown:
    content = Path(path).read_text(encoding="utf-8")
    metadata, body_start = front_matter(content)
    records[path] = metadata
    contents[path] = (content, body_start)

ids = Counter(str(metadata.get("id", "")) for metadata in records.values() if metadata.get("id"))
generated = Counter(semantic_id(path) for path in markdown if not records[path].get("id"))
problems: list[str] = []
for path in selected:
    metadata = records[path]
    for field in ("id", "title", "kind", "status", "domains"):
        if not metadata.get(field):
            problems.append(f"MISSING {path}: {field}")
    if metadata.get("id") and ids[str(metadata["id"])] > 1:
        problems.append(f"DUPLICATE {path}: {metadata['id']}")
    if not metadata.get("id") and generated[semantic_id(path)] > 1:
        problems.append(f"COLLISION {path}: {semantic_id(path)}")
    if metadata.get("kind") and metadata["kind"] not in VALID_KINDS:
        problems.append(f"INVALID {path}: kind={metadata['kind']}")
    if metadata.get("status") and metadata["status"] not in VALID_STATUS:
        problems.append(f"INVALID {path}: status={metadata['status']}")

print(f"元数据扫描：Markdown={len(selected)} 问题={len(problems)}")
if DETAIL:
    for problem in problems:
        print(problem)

collisions = [problem for problem in problems if problem.startswith(("DUPLICATE", "COLLISION"))]
if APPLY and collisions:
    raise SystemExit("存在文档 ID 冲突，未写入元数据。")
if not APPLY:
    raise SystemExit(1 if problems else 0)

updated = 0
for path in selected:
    metadata = records[path]
    content, body_start = contents[path]
    if all(metadata.get(field) for field in ("id", "title", "kind", "status", "domains")):
        continue
    kind, domains = classification(path)
    stable_id = str(metadata.get("id") or semantic_id(path))
    title = str(metadata.get("title") or document_title(content, path)).replace('"', "'")
    kind = str(metadata.get("kind") or kind)
    status = str(metadata.get("status") or "evolving")
    existing_domains = metadata.get("domains")
    if isinstance(existing_domains, list) and existing_domains:
        domains = [str(value) for value in existing_domains]
    header_lines = ["---", f"id: {stable_id}", f'title: "{title}"', f"kind: {kind}", f"status: {status}", "domains:"]
    header_lines.extend(f"  - {domain}" for domain in domains)
    header = "\n".join(header_lines) + "\n---\n"
    body = content[body_start:].lstrip("\r\n") if body_start else content
    separator = "\n" if body else ""
    Path(path).write_text(header + separator + body, encoding="utf-8", newline="")
    updated += 1

print(f"已更新元数据：{updated}")
PY

if [[ $mode == apply ]]; then
    git diff --check
    printf 'Markdown 元数据规范化完成；正文标题未修改。\n'
fi
