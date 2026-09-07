#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Download and verify externally licensed reference files outside Git."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path, PurePosixPath
from typing import Any


repo_root = Path(__file__).resolve().parents[1]
default_manifest_path = repo_root / "reference" / "external_resources" / "arm" / "manifest.json"
sha256_pattern = re.compile(r"^[0-9A-Fa-f]{64}$")
required_document_fields = {
    "id",
    "title",
    "document_number",
    "version",
    "publisher",
    "rights_holder",
    "status",
    "source_kind",
    "official_page",
    "download_url",
    "relative_path",
    "size_bytes",
    "pages",
    "sha256",
    "redistribution",
}
mebibyte = 1024 * 1024
progress_bar_width = 28


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "把第三方参考资料下载到本地缓存，并核对文件大小、PDF 结构、"
            "SHA-256，以及系统存在 pdfinfo 时的页数。"
        )
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=default_manifest_path,
        help="清单路径（默认：reference/external_resources/arm/manifest.json）",
    )
    parser.add_argument(
        "--cache-root",
        type=Path,
        help="覆盖清单声明的本地缓存根目录",
    )
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument(
        "--select",
        action="append",
        default=[],
        metavar="DOCUMENT_ID",
        help="只处理一个文档 ID；可重复指定多个条目",
    )
    selection.add_argument(
        "--profile",
        metavar="PROFILE_ID",
        help="按清单中的学习资料组选取；使用 --list-profiles 查看组名",
    )
    selection.add_argument(
        "--list-profiles",
        action="store_true",
        help="只列出学习资料组，不读取缓存文件也不访问网络",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="展示清单、保存位置和来源，不读取文件也不访问网络",
    )
    parser.add_argument(
        "--verify-only",
        action="store_true",
        help="只校验本地缓存，不下载缺失文件",
    )
    parser.add_argument(
        "--yes",
        action="store_true",
        help="跳过交互确认；适用于已经审阅清单的自动化环境",
    )
    parser.add_argument(
        "--retry-count",
        type=int,
        default=5,
        help="每个文件的最大下载次数（默认：5）",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=60,
        help="网络连接和读取超时秒数（默认：60）",
    )
    args = parser.parse_args()
    if args.retry_count < 1:
        parser.error("--retry-count 必须至少为 1")
    if args.timeout < 1:
        parser.error("--timeout 必须至少为 1")
    return args


def load_manifest(manifest_path: Path) -> dict[str, Any]:
    resolved_path = manifest_path if manifest_path.is_absolute() else repo_root / manifest_path
    with resolved_path.open("r", encoding="utf-8") as manifest_file:
        manifest = json.load(manifest_file)

    if manifest.get("schema_version") != 2:
        raise ValueError("unsupported or missing manifest schema_version")
    if not isinstance(manifest.get("cache_root"), str):
        raise ValueError("manifest cache_root must be a string")
    if not isinstance(manifest.get("documents"), list) or not manifest["documents"]:
        raise ValueError("manifest documents must be a non-empty list")

    document_ids: set[str] = set()
    document_paths: set[str] = set()
    for index, document in enumerate(manifest["documents"]):
        if not isinstance(document, dict):
            raise ValueError(f"document entry {index} must be an object")
        missing_fields = sorted(required_document_fields - document.keys())
        if missing_fields:
            raise ValueError(
                f"document entry {index} is missing fields: {', '.join(missing_fields)}"
            )
        document_id = document["id"]
        if not isinstance(document_id, str) or not document_id:
            raise ValueError(f"document entry {index} has an invalid id")
        if document_id in document_ids:
            raise ValueError(f"duplicate document id: {document_id}")
        document_ids.add(document_id)

        if document["source_kind"] not in {"official_direct", "official_portal"}:
            raise ValueError(f"{document_id}: unsupported source_kind")
        if document["redistribution"] != "not_redistributed":
            raise ValueError(f"{document_id}: redistribution must be not_redistributed")
        if not isinstance(document["size_bytes"], int) or document["size_bytes"] <= 0:
            raise ValueError(f"{document_id}: size_bytes must be a positive integer")
        if not isinstance(document["pages"], int) or document["pages"] <= 0:
            raise ValueError(f"{document_id}: pages must be a positive integer")
        if not isinstance(document["sha256"], str) or not sha256_pattern.fullmatch(
            document["sha256"]
        ):
            raise ValueError(f"{document_id}: sha256 must contain 64 hexadecimal digits")

        for url_field in ("official_page", "download_url"):
            url = document[url_field]
            if url is None and url_field == "download_url":
                continue
            if not isinstance(url, str) or urllib.parse.urlparse(url).scheme != "https":
                raise ValueError(f"{document_id}: {url_field} must use HTTPS")

        relative_path = validate_relative_path(document_id, document["relative_path"])
        # 按 Windows 的大小写不敏感语义检查，禁止两个条目覆盖同一缓存文件。
        path_key = relative_path.as_posix().casefold()
        if path_key in document_paths:
            raise ValueError(f"duplicate document relative_path: {relative_path}")
        document_paths.add(path_key)

    profiles = manifest.get("profiles")
    if not isinstance(profiles, dict) or not profiles:
        raise ValueError("manifest profiles must be a non-empty object")
    for profile_id, profile in profiles.items():
        if not re.fullmatch(r"[a-z][a-z0-9]*(?:_[a-z0-9]+)*", profile_id):
            raise ValueError(f"invalid profile id: {profile_id}")
        if not isinstance(profile, dict):
            raise ValueError(f"{profile_id}: profile must be an object")
        for field in ("title", "description"):
            if not isinstance(profile.get(field), str) or not profile[field].strip():
                raise ValueError(f"{profile_id}: {field} must be a non-empty string")
        profile_ids = profile.get("document_ids")
        if (
            not isinstance(profile_ids, list)
            or not profile_ids
            or any(not isinstance(item, str) for item in profile_ids)
        ):
            raise ValueError(f"{profile_id}: document_ids must be a non-empty string list")
        if len(profile_ids) != len(set(profile_ids)):
            raise ValueError(f"{profile_id}: duplicate document ids")
        unknown_ids = sorted(set(profile_ids) - document_ids)
        if unknown_ids:
            raise ValueError(f"{profile_id}: unknown document ids: {', '.join(unknown_ids)}")

    return manifest


def validate_relative_path(document_id: str, relative_path: Any) -> PurePosixPath:
    if not isinstance(relative_path, str) or not relative_path:
        raise ValueError(f"{document_id}: relative_path must be a non-empty string")
    if "\\" in relative_path:
        raise ValueError(f"{document_id}: relative_path must use forward slashes")

    parsed_path = PurePosixPath(relative_path)
    if (
        parsed_path.is_absolute()
        or ".." in parsed_path.parts
        or any(":" in part for part in parsed_path.parts)
        or not parsed_path.name
    ):
        raise ValueError(f"{document_id}: relative_path must remain below the cache root")
    return parsed_path


def select_documents(
    manifest: dict[str, Any], selected_ids: list[str], profile_id: str | None = None
) -> list[dict[str, Any]]:
    documents = manifest["documents"]
    if profile_id is not None:
        if selected_ids:
            raise ValueError("--profile and --select cannot be combined")
        if profile_id not in manifest["profiles"]:
            raise ValueError(f"unknown profile id: {profile_id}; use --list-profiles")
        selected_ids = manifest["profiles"][profile_id]["document_ids"]
    if not selected_ids:
        return documents

    document_by_id = {document["id"]: document for document in documents}
    unknown_ids = sorted(set(selected_ids) - document_by_id.keys())
    if unknown_ids:
        raise ValueError(f"unknown document ids: {', '.join(unknown_ids)}")
    # 保留选择顺序，同时避免重复参数导致同一文件被重复处理。
    return [document_by_id[document_id] for document_id in dict.fromkeys(selected_ids)]


def list_profiles(manifest: dict[str, Any]) -> None:
    for profile_id, profile in manifest["profiles"].items():
        print(f"{profile_id} — {profile['title']}（{len(profile['document_ids'])} 项）")
        print(f"  {profile['description']}")


def resolve_cache_root(manifest: dict[str, Any], override: Path | None) -> Path:
    if override is not None:
        return override.expanduser().resolve()

    declared_root = Path(manifest["cache_root"])
    if declared_root.is_absolute():
        raise ValueError("manifest cache_root must be repository-relative")
    return (repo_root / declared_root).resolve()


def resolve_destination(cache_root: Path, document: dict[str, Any]) -> Path:
    relative_path = validate_relative_path(document["id"], document["relative_path"])
    destination = (cache_root / Path(*relative_path.parts)).resolve()
    try:
        destination.relative_to(cache_root)
    except ValueError as error:
        raise ValueError(
            f"{document['id']}: destination escapes the cache root"
        ) from error
    return destination


def calculate_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source_file:
        while chunk := source_file.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest().upper()


def format_size(size_bytes: int) -> str:
    if size_bytes < 1024:
        return f"{size_bytes} B"
    if size_bytes < mebibyte:
        return f"{size_bytes / 1024:.1f} KiB"
    if size_bytes < 1024 * mebibyte:
        return f"{size_bytes / mebibyte:.1f} MiB"
    return f"{size_bytes / (1024 * mebibyte):.2f} GiB"


def format_duration(seconds: float | None) -> str:
    if seconds is None or seconds < 0 or seconds == float("inf"):
        return "--:--"
    rounded_seconds = int(seconds + 0.5)
    hours, remainder = divmod(rounded_seconds, 3600)
    minutes, remaining_seconds = divmod(remainder, 60)
    if hours:
        return f"{hours:d}:{minutes:02d}:{remaining_seconds:02d}"
    return f"{minutes:02d}:{remaining_seconds:02d}"


def display_repository_path(path: Path, option_name: str) -> str:
    try:
        relative_path = path.resolve().relative_to(repo_root)
    except ValueError:
        return f"<由 {option_name} 指定的仓库外路径>"
    return relative_path.as_posix() or "."


def display_cache_root(cache_root: Path) -> str:
    return display_repository_path(cache_root, "--cache-root")


def display_destination(cache_root: Path, destination: Path) -> str:
    try:
        relative_path = destination.resolve().relative_to(cache_root)
    except ValueError:
        return "<无效的缓存目标>"
    cache_label = display_cache_root(cache_root)
    relative_label = relative_path.as_posix()
    return f"{cache_label}/{relative_label}" if relative_label else cache_label


def display_error(error: BaseException, path_replacements: list[tuple[Path, str]]) -> str:
    message = str(error)
    for path, replacement in path_replacements:
        resolved_path = path.resolve()
        for path_text in {str(resolved_path), resolved_path.as_posix()}:
            message = message.replace(path_text, replacement)
    return message


def supports_unicode(stream: Any) -> bool:
    encoding = getattr(stream, "encoding", None) or "utf-8"
    try:
        "━▶✓".encode(encoding)
    except (LookupError, UnicodeEncodeError):
        return False
    return True


def heading(title: str) -> None:
    line_character = "━" if supports_unicode(sys.stdout) else "="
    width = min(max(shutil.get_terminal_size((88, 24)).columns, 60), 108)
    print(f"\n{line_character * width}\n{title}\n{line_character * width}")


def acquisition_label(document: dict[str, Any]) -> str:
    return "自动下载" if document["download_url"] else "手工获取"


def document_identity(document: dict[str, Any]) -> str:
    document_number = document["document_number"]
    version = document["version"]
    return f"{document_number} {version}" if version else document_number


def local_file_state(destination: Path) -> str:
    if destination.is_file():
        return "发现本地文件（尚未校验）"
    return "本地缺失"


def inspected_file_state(record: dict[str, Any]) -> str:
    if record["action"] == "skip":
        return "校验通过，将跳过下载"
    if record["action"] == "download" and record["destination"].exists():
        return "校验失败，将先下载并校验新文件，再安全覆盖"
    if record["action"] == "download":
        return "本地缺失，将从官方直链下载"
    if record["action"] == "manual":
        return "需要手工获取，工具不会绕过入口限制"
    return f"校验失败：{record['detail']}"


def print_document_entry(
    document: dict[str, Any],
    cache_root: Path,
    destination: Path,
    position: int,
    total: int,
    show_download_url: bool,
    record: dict[str, Any] | None = None,
) -> None:
    marker = "●" if supports_unicode(sys.stdout) else "*"
    print(
        f"\n{marker} {position:02d}/{total:02d}  "
        f"[{acquisition_label(document)}] {document_identity(document)}"
    )
    print(f"  名称：{document['title']}")
    print(
        f"  发布：{document['publisher']}  |  大小：{format_size(document['size_bytes'])}  "
        f"|  页数：{document['pages']}"
    )
    print(f"  文件：{destination.name}")
    print(f"  保存：{display_destination(cache_root, destination)}")
    state = (
        inspected_file_state(record)
        if record is not None
        else local_file_state(destination)
    )
    print(f"  状态：{state}")
    print(f"  页面：{document['official_page']}")
    if show_download_url and document["download_url"]:
        print(f"  直链：{document['download_url']}")


def print_run_overview(
    manifest_path: Path,
    cache_root: Path,
    documents: list[dict[str, Any]],
    mode: str,
) -> None:
    direct_documents = [document for document in documents if document["download_url"]]
    manual_count = len(documents) - len(direct_documents)
    direct_size = sum(document["size_bytes"] for document in direct_documents)
    heading("linux-note 外部资料获取工具")
    print(f"模式：{mode}")
    print(f"清单：{display_repository_path(manifest_path, '--manifest')}")
    print("链接更新：编辑上述清单中对应 documents[] 条目的 official_page/download_url。")
    print("版本更新：同时核对 version、size_bytes、pages 和 sha256，不能只替换网址。")
    print(f"保存目录：{display_cache_root(cache_root)}")
    print(
        f"条目：共 {len(documents)} 项；自动下载 {len(direct_documents)} 项 "
        f"（合计 {format_size(direct_size)}）；手工获取 {manual_count} 项"
    )
    print("版权：下载文件归各自权利人所有，本仓库不对它们重新授权或再分发。")
    print("Git：本工具不会执行 git add、git commit 或 git push；默认目录位于 .cache。")


def print_document_plan(
    documents: list[dict[str, Any]],
    cache_root: Path,
    show_download_url: bool,
    records: list[dict[str, Any]] | None = None,
) -> None:
    heading("将要处理的文件")
    for position, document in enumerate(documents, start=1):
        record = records[position - 1] if records is not None else None
        print_document_entry(
            document=document,
            cache_root=cache_root,
            destination=resolve_destination(cache_root, document),
            position=position,
            total=len(documents),
            show_download_url=show_download_url,
            record=record,
        )


def confirm_processing(skip_confirmation: bool) -> bool | None:
    if skip_confirmation:
        print("\n确认：已通过 --yes 跳过交互确认。")
        return True
    if not sys.stdin.isatty():
        print("\n未开始：当前环境无法交互确认；审阅上述清单后，请显式添加 --yes。")
        return None

    try:
        answer = input("\n以上内容确认无误后开始处理？输入 y 继续，其他输入取消 [y/N]：")
    except EOFError:
        return False
    return answer.strip().lower() in {"y", "yes"}


def has_pdf_markers(path: Path) -> bool:
    with path.open("rb") as pdf_file:
        header = pdf_file.read(8)
        pdf_file.seek(max(path.stat().st_size - 65536, 0))
        trailer = pdf_file.read()
    return header.startswith(b"%PDF-") and b"%%EOF" in trailer


def read_pdf_page_count(path: Path) -> tuple[int | None, str | None]:
    pdfinfo_path = shutil.which("pdfinfo")
    if pdfinfo_path is None:
        return None, None

    completed_process = subprocess.run(
        [pdfinfo_path, str(path)],
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if completed_process.returncode != 0:
        error_text = completed_process.stderr.strip() or "pdfinfo returned an error"
        return None, error_text

    page_match = re.search(r"^Pages:\s+(\d+)\s*$", completed_process.stdout, re.MULTILINE)
    if page_match is None:
        return None, "pdfinfo did not report a page count"
    return int(page_match.group(1)), None


def verify_file(document: dict[str, Any], path: Path) -> tuple[bool, str]:
    if not path.is_file():
        return False, "file is missing"

    actual_size = path.stat().st_size
    if actual_size != document["size_bytes"]:
        return False, f"size mismatch: expected {document['size_bytes']}, got {actual_size}"
    if not has_pdf_markers(path):
        return False, "PDF header or EOF marker is missing"

    actual_sha256 = calculate_sha256(path)
    expected_sha256 = document["sha256"].upper()
    if actual_sha256 != expected_sha256:
        return False, f"SHA-256 mismatch: expected {expected_sha256}, got {actual_sha256}"

    page_count, page_error = read_pdf_page_count(path)
    if page_error is not None:
        return False, page_error
    if page_count is not None and page_count != document["pages"]:
        return False, f"page mismatch: expected {document['pages']}, got {page_count}"

    page_detail = f", pages={page_count}" if page_count is not None else ", pages=not_checked"
    return True, f"bytes={actual_size}{page_detail}, sha256={actual_sha256}"


class ProgressReporter:
    def __init__(self, expected_bytes: int) -> None:
        self.expected_bytes = expected_bytes
        self.started_at = time.monotonic()
        self.last_update_at = 0.0
        self.last_line_length = 0
        self.next_log_bytes = 0
        self.dynamic = sys.stdout.isatty()

    def build_line(self, downloaded_bytes: int) -> str:
        elapsed = max(time.monotonic() - self.started_at, 0.001)
        percentage = min(downloaded_bytes / self.expected_bytes, 1.0)
        filled_width = min(int(percentage * progress_bar_width), progress_bar_width)
        filled_character = "█" if supports_unicode(sys.stdout) else "#"
        empty_character = "░" if supports_unicode(sys.stdout) else "-"
        progress_bar = (
            filled_character * filled_width
            + empty_character * (progress_bar_width - filled_width)
        )
        bytes_per_second = downloaded_bytes / elapsed
        remaining_bytes = max(self.expected_bytes - downloaded_bytes, 0)
        eta = remaining_bytes / bytes_per_second if bytes_per_second > 0 else None
        return (
            f"  [{progress_bar}] {percentage * 100:6.2f}%  "
            f"{format_size(downloaded_bytes):>10}/{format_size(self.expected_bytes):<10}  "
            f"{format_size(int(bytes_per_second))}/s  ETA {format_duration(eta)}"
        )

    def update(self, downloaded_bytes: int, force: bool = False) -> None:
        current_time = time.monotonic()
        if self.dynamic:
            if not force and current_time - self.last_update_at < 0.08:
                return
            line = self.build_line(downloaded_bytes)
            padding = " " * max(self.last_line_length - len(line), 0)
            print(f"\r{line}{padding}", end="", flush=True)
            self.last_line_length = len(line)
            self.last_update_at = current_time
            if force:
                print()
            return

        if (
            not force
            and downloaded_bytes < self.next_log_bytes
            and current_time - self.last_update_at < 5
        ):
            return
        print(f"  进度：{self.build_line(downloaded_bytes).strip()}", flush=True)
        self.next_log_bytes = downloaded_bytes + 8 * mebibyte
        self.last_update_at = current_time

    def end_interrupted_line(self) -> None:
        if self.dynamic and self.last_line_length:
            print()


def download_to_temporary_file(
    document: dict[str, Any],
    temporary_path: Path,
    timeout: int,
) -> None:
    request = urllib.request.Request(
        document["download_url"],
        headers={"User-Agent": "linux-note-reference-downloader/1"},
    )
    expected_bytes = document["size_bytes"]
    downloaded_bytes = 0
    progress_reporter = ProgressReporter(expected_bytes)

    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            with temporary_path.open("wb") as destination_file:
                progress_reporter.update(0, force=False)
                while chunk := response.read(mebibyte):
                    destination_file.write(chunk)
                    downloaded_bytes += len(chunk)
                    progress_reporter.update(downloaded_bytes)
                destination_file.flush()
                os.fsync(destination_file.fileno())
        progress_reporter.update(downloaded_bytes, force=True)
    except BaseException:
        progress_reporter.end_interrupted_line()
        raise


def inspect_documents(
    documents: list[dict[str, Any]],
    cache_root: Path,
    verify_only: bool,
) -> list[dict[str, Any]]:
    heading("下载前检查")
    records: list[dict[str, Any]] = []
    for position, document in enumerate(documents, start=1):
        destination = resolve_destination(cache_root, document)
        print(
            f"[{position:02d}/{len(documents):02d}] {document_identity(document)}：",
            end="",
            flush=True,
        )
        try:
            verified, detail = verify_file(document, destination)
        except OSError as error:
            verified = False
            destination_label = display_destination(cache_root, destination)
            error_detail = display_error(error, [(destination, destination_label)])
            detail = f"cannot read the target: {error_detail}"
            action = "failed"
            print(f"无法安全读取；{error_detail}")
            records.append(
                {
                    "document": document,
                    "cache_root": cache_root,
                    "destination": destination,
                    "destination_label": destination_label,
                    "verified": verified,
                    "detail": detail,
                    "action": action,
                }
            )
            continue
        if verified:
            action = "skip"
            print("SHA-256 等校验通过，将跳过。")
        elif verify_only:
            action = "failed"
            print(f"校验失败；{detail}")
        elif document["download_url"]:
            action = "download"
            if destination.exists():
                print("已有文件校验失败，将准备安全替换。")
            else:
                print("文件不存在，将准备下载。")
        else:
            action = "manual"
            print("未找到有效文件，需要手工获取。")
        records.append(
            {
                "document": document,
                "cache_root": cache_root,
                "destination": destination,
                "destination_label": display_destination(cache_root, destination),
                "verified": verified,
                "detail": detail,
                "action": action,
            }
        )
    return records


def process_document(
    record: dict[str, Any],
    retry_count: int,
    timeout: int,
    position: int,
    total: int,
) -> str:
    document = record["document"]
    cache_root = record["cache_root"]
    destination = record["destination"]
    destination_label = record["destination_label"]
    print(
        f"\n[{position:02d}/{total:02d}] 处理 "
        f"{document_identity(document)} — {destination.name}"
    )
    print(f"  目标：{destination_label}")
    if record["action"] == "skip":
        print(f"  结果：本地文件校验通过，跳过下载；{record['detail']}")
        return "verified"

    if record["action"] == "failed":
        print(f"  结果：校验失败；{record['detail']}")
        return "failed"

    if record["action"] == "manual":
        print("  结果：需要手工获取；官方入口没有登记可自动使用的稳定直链。")
        print(f"  来源：{document['official_page']}")
        print(f"  放置：合法取得后保存为 {destination_label}")
        print(
            f"  校验：大小 {format_size(document['size_bytes'])}，"
            f"SHA-256 {document['sha256']}"
        )
        return "manual"

    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = destination.with_name(destination.name + ".download")
    last_error = "download did not start"
    print(f"  来源：{document['download_url']}")
    temporary_label = display_destination(cache_root, temporary_path)
    print(f"  临时：{temporary_label}")
    if destination.exists():
        print("  覆盖：旧文件会保留到新文件完成全部校验；校验失败时不会覆盖。")

    for attempt in range(1, retry_count + 1):
        print(f"  下载：第 {attempt}/{retry_count} 次尝试")
        try:
            download_to_temporary_file(document, temporary_path, timeout)
            print("  校验：下载完成，正在检查 PDF、大小、页数和 SHA-256……")
            temporary_verified, temporary_detail = verify_file(document, temporary_path)
            if not temporary_verified:
                raise RuntimeError(temporary_detail)
            os.replace(temporary_path, destination)
            print(f"  结果：下载及校验通过；{temporary_detail}")
            print(f"  文件：{destination_label}")
            return "downloaded"
        except (OSError, RuntimeError, urllib.error.URLError) as error:
            last_error = display_error(
                error,
                [
                    (temporary_path, temporary_label),
                    (destination, destination_label),
                ],
            )
            print(f"  重试：{last_error}", file=sys.stderr)
            if attempt < retry_count:
                retry_delay = min(attempt * 5, 30)
                print(f"  等待：{retry_delay} 秒后重试。", file=sys.stderr)
                time.sleep(retry_delay)

    print(f"  结果：下载失败；{last_error}", file=sys.stderr)
    print(f"  临时文件（如存在）：{temporary_label}", file=sys.stderr)
    return "failed"


def list_documents(documents: list[dict[str, Any]], cache_root: Path) -> None:
    print_document_plan(documents, cache_root, show_download_url=True)


def print_summary(documents: list[dict[str, Any]], results: list[str]) -> None:
    counts = {
        result: results.count(result)
        for result in ("verified", "downloaded", "manual", "failed")
    }
    heading("处理结果")
    print(f"总计：{len(documents)} 项")
    print(f"校验通过：{counts['verified']} 项")
    print(f"新下载：{counts['downloaded']} 项")
    print(f"待手工获取：{counts['manual']} 项")
    print(f"失败：{counts['failed']} 项")
    if counts["manual"]:
        print("提示：手工条目没有被当作下载成功；请按上方官方入口和目标路径处理。")
    if not counts["failed"]:
        print("完成：没有校验失败的条目。脚本未执行任何 Git 操作。")


def main() -> int:
    for stream in (sys.stdout, sys.stderr):
        reconfigure = getattr(stream, "reconfigure", None)
        if reconfigure is not None:
            reconfigure(encoding="utf-8", errors="replace")

    args = parse_args()
    manifest_path = default_manifest_path
    try:
        manifest_path = (
            args.manifest.expanduser().resolve()
            if args.manifest.is_absolute()
            else (repo_root / args.manifest).resolve()
        )
        manifest = load_manifest(args.manifest)
        if args.list_profiles:
            list_profiles(manifest)
            return 0
        documents = select_documents(manifest, args.select, args.profile)
        cache_root = resolve_cache_root(manifest, args.cache_root)
        mode = "仅展示（不访问网络）" if args.list else (
            "只校验（不访问网络）" if args.verify_only else "下载缺失文件并校验"
        )
        print_run_overview(manifest_path, cache_root, documents, mode)
        if args.profile:
            profile = manifest["profiles"][args.profile]
            print(f"学习资料组：{args.profile} — {profile['title']}")
            print(f"阅读边界：{profile['description']}")
        if args.list:
            list_documents(documents, cache_root)
            return 0
    except (OSError, ValueError, json.JSONDecodeError) as error:
        manifest_label = display_repository_path(manifest_path, "--manifest")
        error_detail = display_error(error, [(manifest_path, manifest_label)])
        print(f"[失败] 清单：{error_detail}", file=sys.stderr)
        return 2

    records = inspect_documents(documents, cache_root, args.verify_only)
    print_document_plan(
        documents,
        cache_root,
        show_download_url=False,
        records=records,
    )
    requires_download = any(record["action"] == "download" for record in records)
    if requires_download:
        confirmation = confirm_processing(args.yes)
        if confirmation is None:
            return 2
        if not confirmation:
            print("\n已取消：没有开始下载，也没有修改目标文件。")
            return 0
    if not requires_download and not args.verify_only:
        print("\n检查：没有需要自动下载的文件，不会访问下载直链。")

    heading("执行记录")
    results: list[str] = []
    for position, record in enumerate(records, start=1):
        try:
            result = process_document(
                record=record,
                retry_count=args.retry_count,
                timeout=args.timeout,
                position=position,
                total=len(documents),
            )
        except (OSError, ValueError) as error:
            document_id = record["document"]["id"]
            print(f"  结果：{document_id} 处理失败；{error}", file=sys.stderr)
            result = "failed"
        results.append(result)

    print_summary(documents, results)
    return 1 if "failed" in results else 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\n已中止：用户取消了当前操作。", file=sys.stderr)
        raise SystemExit(130) from None
