#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Regression tests for the external reference downloader."""

from __future__ import annotations

import contextlib
import hashlib
import importlib.util
import io
import tempfile
import unittest
from pathlib import Path
from unittest import mock


repository_root = Path(__file__).resolve().parents[1]
module_path = repository_root / "scripts" / "download_external_resources.py"
module_spec = importlib.util.spec_from_file_location(
    "download_external_resources",
    module_path,
)
if module_spec is None or module_spec.loader is None:
    raise RuntimeError("cannot load download_external_resources.py")
downloader = importlib.util.module_from_spec(module_spec)
module_spec.loader.exec_module(downloader)


def make_document(content: bytes) -> dict[str, object]:
    return {
        "id": "test.document",
        "title": "Test PDF",
        "document_number": "TEST001",
        "version": "1.0",
        "publisher": "Test Publisher",
        "official_page": "https://example.com/document",
        "download_url": "https://example.com/document.pdf",
        "relative_path": "vendor/test_document.pdf",
        "size_bytes": len(content),
        "pages": 1,
        "sha256": hashlib.sha256(content).hexdigest().upper(),
    }


class download_external_resources_tests(unittest.TestCase):
    def test_displayed_paths_do_not_expose_machine_absolute_paths(self) -> None:
        default_cache = repository_root / ".cache" / "private_sources" / "arm"
        default_target = default_cache / "vendor" / "test.pdf"
        self.assertEqual(
            downloader.display_destination(default_cache, default_target),
            ".cache/private_sources/arm/vendor/test.pdf",
        )

        with tempfile.TemporaryDirectory() as temporary_directory:
            custom_cache = Path(temporary_directory).resolve()
            custom_target = custom_cache / "vendor" / "test.pdf"
            displayed_target = downloader.display_destination(custom_cache, custom_target)
            self.assertEqual(
                displayed_target,
                "<由 --cache-root 指定的仓库外路径>/vendor/test.pdf",
            )
            self.assertNotIn(str(custom_cache), displayed_target)

    def test_valid_existing_file_is_skipped(self) -> None:
        valid_content = b"%PDF-1.4\nGOOD\n%%EOF\n"
        document = make_document(valid_content)
        with tempfile.TemporaryDirectory() as temporary_directory:
            cache_root = Path(temporary_directory).resolve()
            destination = cache_root / "vendor" / "test_document.pdf"
            destination.parent.mkdir(parents=True)
            destination.write_bytes(valid_content)

            with (
                mock.patch.object(
                    downloader,
                    "read_pdf_page_count",
                    return_value=(1, None),
                ),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                record = downloader.inspect_documents(
                    [document],
                    cache_root,
                    verify_only=False,
                )[0]

            self.assertEqual(record["action"], "skip")

    def test_invalid_existing_file_is_replaced_only_after_valid_download(self) -> None:
        valid_content = b"%PDF-1.4\nGOOD\n%%EOF\n"
        invalid_content = b"%PDF-1.4\nBAD!\n%%EOF\n"
        self.assertEqual(len(valid_content), len(invalid_content))
        document = make_document(valid_content)

        with tempfile.TemporaryDirectory() as temporary_directory:
            cache_root = Path(temporary_directory).resolve()
            destination = cache_root / "vendor" / "test_document.pdf"
            destination.parent.mkdir(parents=True)
            destination.write_bytes(invalid_content)

            with (
                mock.patch.object(
                    downloader,
                    "read_pdf_page_count",
                    return_value=(1, None),
                ),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                record = downloader.inspect_documents(
                    [document],
                    cache_root,
                    verify_only=False,
                )[0]

                def write_valid_download(
                    selected_document: dict[str, object],
                    temporary_path: Path,
                    timeout: int,
                ) -> None:
                    self.assertIs(selected_document, document)
                    self.assertGreater(timeout, 0)
                    temporary_path.write_bytes(valid_content)

                with mock.patch.object(
                    downloader,
                    "download_to_temporary_file",
                    side_effect=write_valid_download,
                ):
                    result = downloader.process_document(
                        record=record,
                        retry_count=1,
                        timeout=1,
                        position=1,
                        total=1,
                    )

            self.assertEqual(record["action"], "download")
            self.assertEqual(result, "downloaded")
            self.assertEqual(destination.read_bytes(), valid_content)
            self.assertFalse(destination.with_name(destination.name + ".download").exists())

    def test_failed_download_validation_preserves_existing_file(self) -> None:
        valid_content = b"%PDF-1.4\nGOOD\n%%EOF\n"
        invalid_content = b"%PDF-1.4\nBAD!\n%%EOF\n"
        document = make_document(valid_content)

        with tempfile.TemporaryDirectory() as temporary_directory:
            cache_root = Path(temporary_directory).resolve()
            destination = cache_root / "vendor" / "test_document.pdf"
            destination.parent.mkdir(parents=True)
            destination.write_bytes(invalid_content)

            with (
                mock.patch.object(
                    downloader,
                    "read_pdf_page_count",
                    return_value=(1, None),
                ),
                contextlib.redirect_stdout(io.StringIO()),
                contextlib.redirect_stderr(io.StringIO()),
            ):
                record = downloader.inspect_documents(
                    [document],
                    cache_root,
                    verify_only=False,
                )[0]

                def write_invalid_download(
                    selected_document: dict[str, object],
                    temporary_path: Path,
                    timeout: int,
                ) -> None:
                    self.assertIs(selected_document, document)
                    self.assertGreater(timeout, 0)
                    temporary_path.write_bytes(invalid_content)

                with mock.patch.object(
                    downloader,
                    "download_to_temporary_file",
                    side_effect=write_invalid_download,
                ):
                    result = downloader.process_document(
                        record=record,
                        retry_count=1,
                        timeout=1,
                        position=1,
                        total=1,
                    )

            self.assertEqual(record["action"], "download")
            self.assertEqual(result, "failed")
            self.assertEqual(destination.read_bytes(), invalid_content)


if __name__ == "__main__":
    unittest.main()
