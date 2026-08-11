#!/usr/bin/env python3
"""Verify that layout-only edits preserve the tracked Markdown body."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Metrics:
    lines: int = 0
    body_lines: int = 0
    headings: int = 0
    code_blocks: int = 0
    mermaid_blocks: int = 0
    table_rows: int = 0

    def add(self, other: "Metrics") -> None:
        self.lines += other.lines
        self.body_lines += other.body_lines
        self.headings += other.headings
        self.code_blocks += other.code_blocks
        self.mermaid_blocks += other.mermaid_blocks
        self.table_rows += other.table_rows


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Compare a layout-only Markdown topic against a Git baseline and fail "
            "when an original body line or structural artifact disappears."
        )
    )
    parser.add_argument("topic_directory", type=Path)
    parser.add_argument("--repo-root", type=Path)
    parser.add_argument("--baseline", default="HEAD")
    return parser.parse_args()


def find_repo_root(start: Path) -> Path | None:
    for candidate in (start, *start.parents):
        if (candidate / ".git").exists():
            return candidate
    return None


def run_git(repo_root: Path, *arguments: str) -> bytes:
    return subprocess.run(
        ["git", "-C", str(repo_root), *arguments],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    ).stdout


def split_front_matter(lines: list[str]) -> tuple[dict[str, str], list[str]]:
    if not lines or lines[0].strip() != "---":
        return {}, lines
    try:
        end = next(index for index in range(1, len(lines)) if lines[index].strip() == "---")
    except StopIteration:
        return {}, lines

    metadata: dict[str, str] = {}
    for line in lines[1:end]:
        match = re.match(r"^([A-Za-z0-9_]+):\s*(.*)$", line)
        if match:
            metadata[match.group(1)] = match.group(2).strip().strip("\"'")
    return metadata, lines[end + 1 :]


def metrics(text: str) -> Metrics:
    lines = text.splitlines()
    _, body = split_front_matter(lines)
    fence_markers = sum(
        1 for line in lines if re.match(r"^\s*(?:`{3,}|~{3,})", line)
    )
    return Metrics(
        lines=len(lines),
        body_lines=len(body),
        headings=sum(1 for line in lines if re.match(r"^#{1,6}\s", line)),
        code_blocks=fence_markers // 2,
        mermaid_blocks=sum(
            1
            for line in lines
            if re.match(r"^\s*(?:`{3,}|~{3,})\s*mermaid\s*$", line, re.IGNORECASE)
        ),
        table_rows=sum(1 for line in lines if re.match(r"^\s*\|.*\|\s*$", line)),
    )


def preserved_in_order(original: list[str], current: list[str]) -> tuple[bool, int]:
    cursor = 0
    for line in current:
        if cursor < len(original) and line == original[cursor]:
            cursor += 1
    return cursor == len(original), cursor


def main() -> int:
    args = parse_args()
    topic = args.topic_directory.resolve()
    repo_candidate = args.repo_root or find_repo_root(topic)
    if repo_candidate is None:
        print(f"ERROR cannot find repository root from: {topic}")
        return 2
    repo_root = repo_candidate.resolve()
    try:
        relative_topic = topic.relative_to(repo_root).as_posix()
    except ValueError:
        print(f"ERROR topic is outside repository: {topic}")
        return 2

    raw_paths = run_git(
        repo_root,
        "ls-tree",
        "-r",
        "--name-only",
        "-z",
        args.baseline,
        "--",
        relative_topic,
    )
    baseline_paths = [
        item.decode("utf-8", errors="surrogateescape")
        for item in raw_paths.split(b"\0")
        if item and item.lower().endswith(b".md")
    ]
    if not baseline_paths:
        print(f"ERROR baseline has no Markdown files under: {relative_topic}")
        return 2

    errors: list[str] = []
    original_total = Metrics()
    current_total = Metrics()

    for git_path in baseline_paths:
        current_path = repo_root / Path(git_path)
        if not current_path.is_file():
            errors.append(f"missing original path: {git_path}")
            continue

        original_text = run_git(repo_root, "show", f"{args.baseline}:{git_path}").decode(
            "utf-8-sig"
        )
        current_text = current_path.read_text(encoding="utf-8-sig")
        original_lines = original_text.splitlines()
        current_lines = current_text.splitlines()
        original_meta, original_body = split_front_matter(original_lines)
        current_meta, current_body = split_front_matter(current_lines)

        for key in ("id", "title"):
            if original_meta.get(key) != current_meta.get(key):
                errors.append(
                    f"{git_path}: stable metadata changed: {key}="
                    f"{original_meta.get(key)!r} -> {current_meta.get(key)!r}"
                )

        preserved, cursor = preserved_in_order(original_body, current_body)
        if not preserved:
            missing = original_body[cursor] if cursor < len(original_body) else "<unknown>"
            errors.append(
                f"{git_path}: original body is not preserved in order; "
                f"first unmatched line {cursor + 1}: {missing[:120]!r}"
            )

        original_total.add(metrics(original_text))
        current_total.add(metrics(current_text))

    for field in (
        "lines",
        "body_lines",
        "headings",
        "code_blocks",
        "mermaid_blocks",
        "table_rows",
    ):
        original_value = getattr(original_total, field)
        current_value = getattr(current_total, field)
        if current_value < original_value:
            errors.append(
                f"total {field} decreased: {original_value} -> {current_value}"
            )

    for error in errors:
        print(f"ERROR {error}")

    print(
        "SUMMARY "
        f"files={len(baseline_paths)} "
        f"lines={original_total.lines}->{current_total.lines} "
        f"headings={original_total.headings}->{current_total.headings} "
        f"code_blocks={original_total.code_blocks}->{current_total.code_blocks} "
        f"mermaid={original_total.mermaid_blocks}->{current_total.mermaid_blocks} "
        f"table_rows={original_total.table_rows}->{current_total.table_rows} "
        f"errors={len(errors)}"
    )
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
