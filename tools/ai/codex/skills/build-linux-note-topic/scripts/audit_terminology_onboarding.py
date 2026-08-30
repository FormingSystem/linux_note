#!/usr/bin/env python3
"""Report mechanically observable first-use risks for technical terminology.

The audit inventories uppercase abbreviations, CONFIG_* names, and identifier-like
inline-code spans.  It checks only whether the nearby introduction has signals for
a bilingual expansion or an explicit name/type explanation.  It cannot decide
whether an explanation is technically correct, discover every subsystem nickname,
or prove that definitions contain no unknown prerequisites.  Manual novice cold
reading remains mandatory.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


ACRONYM_RE = re.compile(
    r"(?<![A-Za-z0-9_])(?:CONFIG_[A-Z0-9_]+|[A-Z][A-Z0-9]*(?:[_/-][A-Z0-9]+)*)(?![A-Za-z0-9_])"
)
INLINE_CODE_RE = re.compile(r"`([^`\n]+)`")
IDENTIFIER_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*(?:\(\))?$")
CJK_RE = re.compile(r"[\u3400-\u9fff]")
ENGLISH_PHRASE_RE = re.compile(
    r"\b[A-Za-z][A-Za-z]*(?:[- ][A-Za-z][A-Za-z]*){1,}\b"
)
MARKDOWN_LINK_RE = re.compile(r"!?\[([^\]]*)\]\(([^)]+)\)")
FENCE_RE = re.compile(r"^\s*```\s*([^\s`]*)")
HEADING_RE = re.compile(r"^\s*#{1,6}\s+")
TABLE_RE = re.compile(r"^\s*\|")

NAME_KIND_WORDS = (
    "缩写",
    "专名",
    "名称",
    "历史命名",
    "昵称",
    "标识符",
    "接口",
    "API",
    "函数",
    "宏",
    "字段",
    "变量",
    "参数",
    "结构体",
    "类型",
    "配置",
    "Kconfig",
    "选项",
    "符号",
    "命令",
    "工具",
    "格式",
    "实现家族",
    "执行模型",
    "诊断",
)
CONFIG_KIND_WORDS = ("Kconfig", "配置", "选项", "配置符号")
IGNORED_TERMS = {
    "TODO",
    "FIXME",
    "XXX",
    "TRUE",
    "FALSE",
    "NULL",
}
IGNORED_IDENTIFIER_TERMS = {
    "c",
    "cc",
    "cpp",
    "text",
    "bash",
    "sh",
    "shell",
    "true",
    "false",
    "null",
    "none",
    "yes",
    "no",
    "y",
    "n",
}


@dataclass
class Candidate:
    term: str
    kind: str
    line: int
    context: str
    count: int = 1


@dataclass(frozen=True)
class Issue:
    code: str
    term: str
    line: int
    context: str
    message: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Audit first-use onboarding signals for terminology in Markdown chapters."
    )
    parser.add_argument("target", type=Path, help="Markdown chapter or topic directory")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Return failure when any mechanically visible terminology risk is detected.",
    )
    return parser.parse_args()


def chapter_paths(target: Path) -> list[Path]:
    if target.is_file() and target.suffix.lower() == ".md":
        return [target]
    if target.is_dir():
        return sorted(target.rglob("P[0-9][0-9]_*.md"))
    return []


def without_front_matter(lines: list[str]) -> list[str]:
    result = list(lines)
    if not result or result[0].strip() != "---":
        return result
    for index in range(1, len(result)):
        if result[index].strip() == "---":
            for hidden in range(index + 1):
                result[hidden] = ""
            break
    return result


def visible_line(line: str) -> str:
    value = MARKDOWN_LINK_RE.sub(lambda match: match.group(1), line)
    value = re.sub(r"<a\s+[^>]*></a>", "", value)
    return value


def line_contexts(lines: list[str]) -> list[str]:
    contexts: list[str] = []
    fence_kind = ""
    for line in lines:
        fence = FENCE_RE.match(line)
        if fence:
            fence_kind = "" if fence_kind else fence.group(1).casefold()
            contexts.append("fence")
            continue
        if fence_kind:
            contexts.append("mermaid" if fence_kind == "mermaid" else "code")
        elif HEADING_RE.match(line):
            contexts.append("heading")
        elif TABLE_RE.match(line):
            contexts.append("table")
        else:
            contexts.append("prose")
    return contexts


def ignored_acronym(term: str) -> bool:
    if term in IGNORED_TERMS:
        return True
    if re.fullmatch(r"[HPS]\d+", term):
        return True
    if len(term) < 2 or sum(character.isalpha() for character in term) < 2:
        return True
    return False


def identifier_term(value: str) -> str | None:
    value = value.strip()
    if not IDENTIFIER_RE.fullmatch(value):
        return None
    normalized = value[:-2] if value.endswith("()") else value
    if normalized.casefold() in IGNORED_IDENTIFIER_TERMS:
        return None
    return normalized


def candidate_kind(term: str, from_inline_code: bool, context: str) -> str:
    if term.startswith("CONFIG_"):
        return "config"
    if from_inline_code or "_" in term or context == "code":
        return "identifier"
    return "acronym"


def collect_candidates(lines: list[str], contexts: list[str]) -> dict[str, Candidate]:
    candidates: dict[str, Candidate] = {}
    for index, raw_line in enumerate(lines):
        if contexts[index] == "fence":
            continue
        line = visible_line(raw_line)
        found: list[tuple[str, bool]] = []
        for match in ACRONYM_RE.finditer(line):
            term = match.group(0)
            if not ignored_acronym(term):
                found.append((term, False))
        for match in INLINE_CODE_RE.finditer(line):
            term = identifier_term(match.group(1))
            if term:
                found.append((term, True))

        line_terms: dict[str, bool] = {}
        for term, from_inline_code in found:
            line_terms[term] = line_terms.get(term, False) or from_inline_code
        for term, from_inline_code in line_terms.items():
            kind = candidate_kind(term, from_inline_code, contexts[index])
            existing = candidates.get(term)
            if existing:
                existing.count += 1
                if existing.kind == "acronym" and kind != "acronym":
                    existing.kind = kind
                continue
            candidates[term] = Candidate(
                term=term,
                kind=kind,
                line=index + 1,
                context=contexts[index],
            )
    return candidates


def window_text(lines: list[str], start: int, end: int) -> str:
    return "\n".join(visible_line(line) for line in lines[start:end])


def has_kind_signal(text: str, words: tuple[str, ...]) -> bool:
    return any(word in text for word in words)


def has_english_full_name_signal(term: str, text: str) -> bool:
    for match in ENGLISH_PHRASE_RE.finditer(text):
        phrase = match.group(0)
        if phrase.casefold() == term.casefold():
            continue
        components = [item for item in re.split(r"[- ]+", phrase) if item]
        if len(components) < 2:
            continue
        distance = min(abs(match.start() - item.start()) for item in re.finditer(re.escape(term), text))
        if distance <= 180:
            return True
    return False


def has_precise_link_signal(term: str, text: str) -> bool:
    for match in MARKDOWN_LINK_RE.finditer(text):
        label, target = match.groups()
        if term in label and "#" in target:
            return True
    return False


def audit_candidate(candidate: Candidate, lines: list[str]) -> list[Issue]:
    first_index = candidate.line - 1
    nearby = window_text(lines, max(0, first_index - 3), min(len(lines), first_index + 9))
    before_or_same = window_text(lines, max(0, first_index - 8), first_index + 1)
    cjk = bool(CJK_RE.search(nearby))
    issues: list[Issue] = []

    if candidate.kind == "acronym":
        full_name = has_english_full_name_signal(candidate.term, nearby)
        if not cjk or not full_name:
            missing = []
            if not cjk:
                missing.append("accurate Chinese meaning")
            if not full_name:
                missing.append("English full-name signal")
            issues.append(
                Issue(
                    "ACRONYM_WITHOUT_BILINGUAL_ENTRY",
                    candidate.term,
                    candidate.line,
                    candidate.context,
                    "first-use window lacks " + " and ".join(missing),
                )
            )
    else:
        words = CONFIG_KIND_WORDS if candidate.kind == "config" else NAME_KIND_WORDS
        kind_signal = has_kind_signal(nearby, words)
        if not cjk or not kind_signal:
            missing = []
            if not cjk:
                missing.append("Chinese local explanation")
            if not kind_signal:
                missing.append("name/type signal")
            issue_code = (
                "CONFIG_WITHOUT_KCONFIG_ENTRY"
                if candidate.kind == "config"
                else "IDENTIFIER_WITHOUT_TYPE_ENTRY"
            )
            issues.append(
                Issue(
                    issue_code,
                    candidate.term,
                    candidate.line,
                    candidate.context,
                    "first-use window lacks " + " and ".join(missing),
                )
            )

    if candidate.context in {"table", "mermaid", "code"}:
        if candidate.kind == "acronym":
            introduced_before = bool(CJK_RE.search(before_or_same)) and has_english_full_name_signal(
                candidate.term, before_or_same
            )
        else:
            words = CONFIG_KIND_WORDS if candidate.kind == "config" else NAME_KIND_WORDS
            introduced_before = bool(CJK_RE.search(before_or_same)) and has_kind_signal(
                before_or_same, words
            )
        if not introduced_before:
            issues.append(
                Issue(
                    "NON_PROSE_FIRST_USE_BEFORE_ENTRY",
                    candidate.term,
                    candidate.line,
                    candidate.context,
                    "term first appears in a table/diagram/code context before an observable entry",
                )
            )
    return issues


def audit(path: Path) -> tuple[list[Candidate], list[Issue], int]:
    raw_lines = path.read_text(encoding="utf-8-sig").splitlines()
    lines = without_front_matter(raw_lines)
    contexts = line_contexts(lines)
    candidates = sorted(
        collect_candidates(lines, contexts).values(), key=lambda item: (item.line, item.term)
    )
    issues: list[Issue] = []
    precise_links = 0
    for candidate in candidates:
        issues.extend(audit_candidate(candidate, lines))
        first_index = candidate.line - 1
        nearby = "\n".join(raw_lines[max(0, first_index - 3) : min(len(raw_lines), first_index + 9)])
        if has_precise_link_signal(candidate.term, nearby):
            precise_links += 1
    return candidates, issues, precise_links


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

    total_candidates = 0
    total_issues = 0
    for path in paths:
        try:
            candidates, issues, precise_links = audit(path)
        except (OSError, UnicodeError) as exc:
            print(f"ERROR {path}: could not read chapter: {exc}")
            total_issues += 1
            continue

        total_candidates += len(candidates)
        total_issues += len(issues)
        print(
            f"METRIC {path}: term_candidates={len(candidates)} "
            f"precise_link_signals={precise_links} risks={len(issues)}"
        )
        for issue in issues:
            level = "ERROR" if args.strict else "WARN "
            print(
                f"{level} {path}:{issue.line}: {issue.code}: term={issue.term} "
                f"context={issue.context}: {issue.message}"
            )

    print(
        f"SUMMARY chapters={len(paths)} candidates={total_candidates} risks={total_issues} "
        f"strict={str(args.strict).lower()} manual_private_name_review_required=true "
        "manual_definition_dependency_review_required=true"
    )
    return 1 if args.strict and total_issues else 0


if __name__ == "__main__":
    sys.exit(main())
