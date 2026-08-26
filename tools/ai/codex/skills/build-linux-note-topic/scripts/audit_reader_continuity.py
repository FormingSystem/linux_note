#!/usr/bin/env python3
"""Report mechanically observable risks to continuous chapter reading.

This audit is intentionally conservative.  It can reject obvious link routing,
decorative heading hierarchy, source-title mismatch, and oversized sections,
but it cannot prove that a chapter has a sound cognitive path.  The skill's
cold-read review remains mandatory.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
HEADING_RE = re.compile(r"^(#{1,6})\s+(.+?)\s*$")
SOURCE_PROMISE_RE = re.compile(r"源码(?:实现|讲解|机制|详解|同步机制)")
SOURCE_EVIDENCE_HEADING_RE = re.compile(
    r"源码摘录|上游源码|实现原理|实现验证|源码证据|函数实现|字段实现"
)


@dataclass(frozen=True)
class Issue:
    code: str
    message: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Audit link dominance and title-contract risks in Markdown chapters."
    )
    parser.add_argument("target", type=Path, help="Markdown chapter or topic directory")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Return failure when any continuity risk is detected.",
    )
    return parser.parse_args()


def chapter_paths(target: Path) -> list[Path]:
    if target.is_file():
        return [target]
    if target.is_dir():
        return sorted(target.rglob("P[0-9][0-9]_*.md"))
    return []


def outside_fence_lines(text: str) -> list[tuple[int, str]]:
    result: list[tuple[int, str]] = []
    in_fence = False
    for number, line in enumerate(text.splitlines(), 1):
        if line.startswith("```"):
            in_fence = not in_fence
            continue
        if not in_fence:
            result.append((number, line))
    return result


def heading_records(text: str) -> list[tuple[int, int, str]]:
    result: list[tuple[int, int, str]] = []
    for number, line in outside_fence_lines(text):
        match = HEADING_RE.match(line)
        if match:
            result.append((number, len(match.group(1)), match.group(2)))
    return result


def internal_links(text: str) -> list[str]:
    result: list[str] = []
    for match in LINK_RE.finditer(text):
        target = match.group(1).strip()
        if target.startswith(("http://", "https://", "mailto:", "data:")):
            continue
        result.append(target)
    return result


def first_reading_section(text: str, headings: list[tuple[int, int, str]]) -> str:
    lines = text.splitlines()
    h2_lines = [number for number, level, _ in headings if level == 2]
    if len(h2_lines) >= 2:
        return "\n".join(lines[: h2_lines[1] - 1])
    return text


def narrative_character_count(text: str) -> int:
    value = re.sub(r"^---\s*\n.*?\n---\s*(?:\n|$)", "", text, flags=re.S)
    total = 0
    for _, line in outside_fence_lines(value):
        stripped = line.strip()
        if (
            not stripped
            or stripped.startswith("#")
            or stripped.startswith("|")
            or re.match(r"^[-*+]\s+.*\[[^\]]+\]\(", stripped)
        ):
            continue
        stripped = re.sub(r"!?\[([^\]]*)\]\([^)]+\)", r"\1", stripped)
        total += len(re.findall(r"[A-Za-z0-9_\u3400-\u9fff]", stripped))
    return total


def largest_h2_span(text: str, headings: list[tuple[int, int, str]]) -> tuple[int, str]:
    h2 = [(number, title) for number, level, title in headings if level == 2]
    if not h2:
        return 0, ""
    total = len(text.splitlines())
    largest = (0, "")
    for index, (start, title) in enumerate(h2):
        end = h2[index + 1][0] - 1 if index + 1 < len(h2) else total
        span = end - start + 1
        if span > largest[0]:
            largest = (span, title)
    return largest


def c_block_headings(text: str) -> list[str]:
    headings: list[str] = []
    for match in re.finditer(r"```c(?:[ \t]+[^\n]*)?\n.*?```", text, re.S):
        preceding = heading_records(text[: match.start()])
        headings.append(preceding[-1][2] if preceding else "")
    return headings


def title_text(text: str, headings: list[tuple[int, int, str]]) -> str:
    h1 = next((title for _, level, title in headings if level == 1), "")
    meta_title = ""
    if text.startswith("---"):
        front = re.match(r"^---\s*\n(.*?)\n---", text, re.S)
        if front:
            item = re.search(r'^title:\s*["\']?(.*?)["\']?\s*$', front.group(1), re.M)
            if item:
                meta_title = item.group(1)
    return f"{h1} {meta_title}"


def normalized_heading_task(title: str) -> str:
    value = re.sub(r"\\([\\`*_{}\[\]()#+\-.!])", r"\1", title)
    value = re.sub(r"`([^`]*)`", r"\1", value)
    value = re.sub(r"\*\*([^*]+)\*\*", r"\1", value)
    value = re.sub(r"^(?:第\d+章|(?:\d+\.)+\d+)[_：:.\s-]*", "", value)
    return re.sub(r"[_`#\s：:，,。.!！？?（）()\[\]【】<>《》/|\\-]+", "", value).casefold()


def empty_h2_titles(text: str, headings: list[tuple[int, int, str]]) -> list[str]:
    lines = text.splitlines()
    result: list[str] = []
    for index, (start, level, title) in enumerate(headings):
        if level != 2:
            continue
        end = headings[index + 1][0] - 1 if index + 1 < len(headings) else len(lines)
        body = [
            line.strip()
            for line in lines[start:end]
            if line.strip()
            and not re.fullmatch(r"<!--[\s\S]*-->", line.strip())
            and not re.fullmatch(r"<a\s+[^>]*></a>", line.strip())
        ]
        if not body:
            result.append(title)
    return result


def audit(path: Path) -> tuple[dict[str, int | str], list[Issue]]:
    text = path.read_text(encoding="utf-8-sig")
    headings = heading_records(text)
    all_internal_links = internal_links(text)
    opening_section = first_reading_section(text, headings)
    opening_internal_links = internal_links(opening_section)
    opening_narrative_chars = narrative_character_count(opening_section)
    h2_count = sum(1 for _, level, _ in headings if level == 2)
    largest_span, largest_title = largest_h2_span(text, headings)
    source_headings = c_block_headings(text)
    is_source_explanation = "source_explanations" in path.parts
    source_evidence_blocks = (
        len(source_headings)
        if is_source_explanation
        else sum(1 for heading in source_headings if SOURCE_EVIDENCE_HEADING_RE.search(heading))
    )
    promised_source = bool(SOURCE_PROMISE_RE.search(title_text(text, headings)))
    h1_title = next((title for _, level, title in headings if level == 1), "")
    first_h2_title = next((title for _, level, title in headings if level == 2), "")
    empty_h2 = empty_h2_titles(text, headings)
    issues: list[Issue] = []

    if (
        h1_title
        and first_h2_title
        and normalized_heading_task(h1_title) == normalized_heading_task(first_h2_title)
    ):
        issues.append(
            Issue(
                "TITLE_ECHO",
                "first H2 repeats the H1 reader task; replace it with a real local question or remove it",
            )
        )

    for empty_title in empty_h2:
        issues.append(
            Issue(
                "EMPTY_H2",
                f"H2 has no framing content before the next heading ({empty_title}); "
                "explain the section transition or remove the decorative heading",
            )
        )

    if len(opening_internal_links) >= 6 and opening_narrative_chars < 160:
        issues.append(
            Issue(
                "LINK_DOMINATED_OPENING",
                f"opening contains {len(opening_internal_links)} internal links but only "
                f"{opening_narrative_chars} narrative characters before the second H2; "
                "establish the reader task before source routing",
            )
        )

    link_limit = max(36, h2_count * 5) if is_source_explanation else max(20, h2_count * 3)
    if len(all_internal_links) > link_limit:
        issues.append(
            Issue(
                "LINK_DENSITY",
                f"chapter contains {len(all_internal_links)} internal links for {h2_count} H2 sections "
                f"(risk threshold {link_limit}); verify that links only deepen locally complete explanations",
            )
        )

    oversized_limit = 200 if is_source_explanation else 160
    if largest_span >= oversized_limit:
        issues.append(
            Issue(
                "OVERSIZED_MODULE",
                f"largest H2 section spans {largest_span} lines ({largest_title}); "
                f"risk threshold is {oversized_limit}; check whether several modules or repeated "
                "representations are mixed together",
            )
        )

    if promised_source and source_evidence_blocks == 0:
        issues.append(
            Issue(
                "SOURCE_TITLE_WITHOUT_LOCAL_EVIDENCE",
                "title promises source implementation/mechanism, but no C excerpt appears under a "
                "source/implementation/evidence heading; links and application examples do not satisfy the title",
            )
        )

    metrics: dict[str, int | str] = {
        "lines": len(text.splitlines()),
        "h2": h2_count,
        "internal_links": len(all_internal_links),
        "opening_internal_links": len(opening_internal_links),
        "opening_narrative_chars": opening_narrative_chars,
        "largest_h2_lines": largest_span,
        "source_evidence_c_blocks": source_evidence_blocks,
        "empty_h2": len(empty_h2),
    }
    return metrics, issues


def main() -> int:
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8")
    args = parse_args()
    target = args.target.resolve()
    paths = chapter_paths(target)
    if not paths:
        print(f"ERROR no Markdown chapters found: {target}")
        return 2

    issue_count = 0
    for path in paths:
        try:
            metrics, issues = audit(path)
        except (OSError, UnicodeError) as exc:
            print(f"ERROR {path}: could not read chapter: {exc}")
            issue_count += 1
            continue

        metric_text = " ".join(f"{key}={value}" for key, value in metrics.items())
        print(f"METRIC {path}: {metric_text}")
        for issue in issues:
            level = "ERROR" if args.strict else "WARN "
            print(f"{level} {path}: {issue.code}: {issue.message}")
        issue_count += len(issues)

    print(
        f"SUMMARY chapters={len(paths)} risks={issue_count} strict={str(args.strict).lower()} "
        "manual_cold_read_required=true"
    )
    return 1 if args.strict and issue_count else 0


if __name__ == "__main__":
    sys.exit(main())
