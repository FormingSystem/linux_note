#!/usr/bin/env python3
"""Read-only structural audit for a linux-note topic directory."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from urllib.parse import unquote


REQUIRED_META = ("id", "title", "kind", "status", "domains")
SKIP_DIRS = {
    ".git",
    ".local",
    "node_modules",
    "dist",
    "out",
    "build",
    "cmake-build-debug",
    "cmake-build-release",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Audit linux-note topic metadata, numbering, navigation, links, and diagrams."
    )
    parser.add_argument("topic_directory", type=Path)
    parser.add_argument("--repo-root", type=Path)
    return parser.parse_args()


def find_repo_root(start: Path) -> Path | None:
    for candidate in (start, *start.parents):
        if (candidate / ".git").exists():
            return candidate
    return None


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def front_matter(text: str) -> dict[str, str]:
    if not text.startswith("---"):
        return {}
    match = re.match(r"^---\s*\n(.*?)\n---\s*(?:\n|$)", text, re.S)
    if not match:
        return {}
    result: dict[str, str] = {}
    for line in match.group(1).splitlines():
        item = re.match(r"^([a-zA-Z0-9_]+):\s*(.*)$", line)
        if item:
            result[item.group(1)] = item.group(2).strip().strip('"\'')
    return result


def lines_outside_fences(text: str) -> list[tuple[int, str]]:
    result: list[tuple[int, str]] = []
    in_fence = False
    for number, line in enumerate(text.splitlines(), 1):
        if line.startswith("```"):
            in_fence = not in_fence
            continue
        if not in_fence:
            result.append((number, line))
    return result


def extract_links(text: str) -> list[str]:
    return [match.group(1).strip() for match in re.finditer(r"!?\[[^\]]*\]\(([^)]+)\)", text)]


def normalize_link_target(raw: str) -> str | None:
    if not raw or raw.startswith(("#", "http://", "https://", "mailto:", "data:")):
        return None
    target = raw
    if target.startswith("<") and ">" in target:
        target = target[1 : target.index(">")]
    else:
        target = target.split(maxsplit=1)[0]
    target = unquote(target.split("#", 1)[0])
    return target or None


def iter_repo_markdown(repo_root: Path):
    for path in repo_root.rglob("*.md"):
        if any(part in SKIP_DIRS for part in path.parts):
            continue
        yield path


def main() -> int:
    args = parse_args()
    topic = args.topic_directory.resolve()
    if not topic.is_dir():
        print(f"ERROR topic directory does not exist: {topic}")
        return 2

    repo_root = args.repo_root.resolve() if args.repo_root else find_repo_root(topic)
    files = sorted(topic.glob("P[0-9][0-9]_*.md"))
    outline = topic / "大纲.md"
    topic_files = ([outline] if outline.exists() else []) + files
    errors: list[str] = []
    warnings: list[str] = []
    ids: dict[str, Path] = {}
    mermaid_count = 0
    has_c_block = False
    has_sequence_diagram = False
    has_role_graph = False

    if not outline.exists():
        errors.append(f"missing topic outline: {outline}")
    if not files:
        errors.append(f"no PXX Markdown chapters found: {topic}")

    for path in topic_files:
        text = read_text(path)
        meta = front_matter(text)
        for key in REQUIRED_META:
            if key not in meta:
                errors.append(f"{path}: missing front-matter field {key}")
        if meta.get("id"):
            previous = ids.get(meta["id"])
            if previous:
                errors.append(f"duplicate topic id {meta['id']}: {previous} and {path}")
            ids[meta["id"]] = path

        fence_count = sum(1 for line in text.splitlines() if line.startswith("```"))
        if fence_count % 2:
            errors.append(f"{path}: unbalanced fenced code block")

        previous_level = 0
        for number, line in lines_outside_fences(text):
            heading = re.match(r"^(#{1,6})\s+", line)
            if not heading:
                continue
            level = len(heading.group(1))
            if previous_level and level > previous_level + 1:
                errors.append(f"{path}:{number}: heading jump {previous_level} -> {level}")
            previous_level = level
            if "_" in line and "\\_" not in line:
                warnings.append(f"{path}:{number}: heading contains an unescaped underscore")

        mermaid_blocks = re.findall(r"```mermaid\s*\n(.*?)```", text, re.S)
        mermaid_count += len(mermaid_blocks)
        for index, block in enumerate(mermaid_blocks, 1):
            if "\\n" in block:
                errors.append(f"{path}: Mermaid block {index} uses literal \\n instead of <br/>")

        for raw_target in extract_links(text):
            target = normalize_link_target(raw_target)
            if target is None:
                continue
            resolved = (path.parent / target).resolve()
            if not resolved.exists():
                errors.append(f"{path}: broken relative link -> {raw_target}")

        for number, line in enumerate(text.splitlines(), 1):
            if re.search(r"[ \t]+$", line):
                errors.append(f"{path}:{number}: trailing whitespace")

        chapter_match = re.match(r"^P(\d{2})_", path.name)
        if not chapter_match:
            continue
        chapter = int(chapter_match.group(1))
        h1 = next((line for _, line in lines_outside_fences(text) if line.startswith("# ")), "")
        if not re.match(rf"^# 第{chapter}章\\_", h1):
            errors.append(f"{path}: H1 does not match chapter {chapter}: {h1!r}")

        h2_numbers = []
        for _, line in lines_outside_fences(text):
            match = re.match(rf"^## {chapter}\.(\d+)\\_", line)
            if match:
                h2_numbers.append(int(match.group(1)))
        if h2_numbers != list(range(1, len(h2_numbers) + 1)):
            errors.append(f"{path}: non-sequential H2 numbers: {h2_numbers}")

        has_c_block = has_c_block or "```c" in text
        has_sequence_diagram = has_sequence_diagram or "sequenceDiagram" in text
        has_role_graph = has_role_graph or bool(re.search(r"(?:flowchart|graph)\s", text))

    if files and not has_c_block:
        warnings.append(f"{topic}: topic has no C scenario/source block")
    if files and not has_sequence_diagram:
        warnings.append(f"{topic}: topic has no end-to-end Mermaid sequence diagram")
    if files and not has_role_graph:
        warnings.append(f"{topic}: topic has no Mermaid role/state relationship graph")

    if outline.exists():
        outline_targets = {
            Path(target).name
            for raw in extract_links(read_text(outline))
            if (target := normalize_link_target(raw)) and target.endswith(".md")
        }
        for path in files:
            if path.name not in outline_targets:
                errors.append(f"outline does not link chapter: {path.name}")

    for index, path in enumerate(files):
        text = read_text(path)
        if index > 0:
            expected = files[index - 1].name
            match = re.search(r"^上一篇：.*?\]\(([^)#]+)", text, re.M)
            if not match or Path(unquote(match.group(1))).name != expected:
                errors.append(f"{path}: previous navigation should target {expected}")
        if index < len(files) - 1:
            expected = files[index + 1].name
            match = re.search(r"^下一篇：.*?\]\(([^)#]+)", text, re.M)
            if not match or Path(unquote(match.group(1))).name != expected:
                errors.append(f"{path}: next navigation should target {expected}")

    if repo_root and repo_root.is_dir():
        global_ids: dict[str, Path] = {}
        for path in iter_repo_markdown(repo_root):
            try:
                document_id = front_matter(read_text(path)).get("id")
            except (OSError, UnicodeError) as exc:
                warnings.append(f"{path}: could not read for ID audit: {exc}")
                continue
            if not document_id:
                continue
            previous = global_ids.get(document_id)
            if previous:
                errors.append(f"duplicate repository id {document_id}: {previous} and {path}")
            global_ids[document_id] = path
    else:
        warnings.append("repository root not found; skipped global duplicate-ID audit")

    for item in errors:
        print(f"ERROR {item}")
    for item in warnings:
        print(f"WARN  {item}")
    print(
        f"SUMMARY chapters={len(files)} markdown={len(topic_files)} "
        f"mermaid={mermaid_count} errors={len(errors)} warnings={len(warnings)}"
    )
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
