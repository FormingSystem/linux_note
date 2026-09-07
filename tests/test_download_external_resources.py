#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Regression tests for the external reference downloader."""

from __future__ import annotations

import contextlib
import hashlib
import importlib.util
import io
import json
import sys
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
        "rights_holder": "Test Publisher",
        "status": "reference",
        "source_kind": "official_direct",
        "redistribution": "not_redistributed",
        "official_page": "https://example.com/document",
        "download_url": "https://example.com/document.pdf",
        "relative_path": "vendor/test_document.pdf",
        "size_bytes": len(content),
        "pages": 1,
        "sha256": hashlib.sha256(content).hexdigest().upper(),
    }


def make_manifest() -> dict[str, object]:
    first = make_document(b"%PDF-1.4\nGOOD\n%%EOF\n")
    second = dict(first, id="test.second", relative_path="vendor/second.pdf")
    return {
        "schema_version": 2,
        "cache_root": ".cache/private_sources/test",
        "documents": [first, second],
        "profiles": {
            "test_profile": {
                "title": "测试资料组",
                "description": "测试顺序与共享文档",
                "document_ids": ["test.second", "test.document"],
            }
        },
    }


class download_external_resources_tests(unittest.TestCase):
    def load_test_manifest(self, manifest: dict[str, object]) -> dict[str, object]:
        # 测试夹具写入临时目录，不触碰真实资料缓存。
        with tempfile.TemporaryDirectory() as temporary_directory:
            path = Path(temporary_directory) / "manifest.json"
            path.write_text(json.dumps(manifest), encoding="utf-8")
            return downloader.load_manifest(path)

    def test_repository_manifest_profiles_are_valid(self) -> None:
        manifest = downloader.load_manifest(downloader.default_manifest_path)
        profile = manifest["profiles"]["rk3588_gicv3"]
        self.assertIn("arm.gic600.100336_0106_00", profile["document_ids"])
        self.assertNotIn("arm.gic.virtualization.107627_0102_02", profile["document_ids"])

    def test_profile_preserves_reading_order_without_copying_documents(self) -> None:
        manifest = self.load_test_manifest(make_manifest())
        selected = downloader.select_documents(manifest, [], "test_profile")
        self.assertIs(selected[0], manifest["documents"][1])
        self.assertIs(selected[1], manifest["documents"][0])

    def test_default_selection_remains_complete_collection(self) -> None:
        manifest = make_manifest()
        self.assertIs(downloader.select_documents(manifest, []), manifest["documents"])

    def test_repeated_document_selection_is_deduplicated(self) -> None:
        selected = downloader.select_documents(
            make_manifest(), ["test.second", "test.second", "test.document"]
        )
        self.assertEqual([item["id"] for item in selected], ["test.second", "test.document"])

    def test_unknown_profile_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "unknown profile"):
            downloader.select_documents(make_manifest(), [], "missing_profile")

    def test_conflicting_selectors_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "cannot be combined"):
            downloader.select_documents(make_manifest(), ["test.document"], "test_profile")
        with (
            mock.patch.object(sys, "argv", ["downloader", "--profile", "test_profile", "--select", "test.document"]),
            contextlib.redirect_stderr(io.StringIO()),
            self.assertRaises(SystemExit) as raised,
        ):
            downloader.parse_args()
        self.assertEqual(raised.exception.code, 2)

    def test_invalid_profiles_are_rejected(self) -> None:
        invalid_profiles = [None, {}, {"bad-name": {}}, {"test_profile": []}]
        for profiles in invalid_profiles:
            with self.subTest(profiles=profiles), self.assertRaises(ValueError):
                self.load_test_manifest(dict(make_manifest(), profiles=profiles))

    def test_invalid_profile_members_are_rejected(self) -> None:
        for members in ([], [123], ["unknown"], ["test.document", "test.document"]):
            manifest = make_manifest()
            manifest["profiles"]["test_profile"]["document_ids"] = members
            with self.subTest(members=members), self.assertRaises(ValueError):
                self.load_test_manifest(manifest)

    def test_profile_requires_explanation(self) -> None:
        manifest = make_manifest()
        manifest["profiles"]["test_profile"]["description"] = " "
        with self.assertRaisesRegex(ValueError, "description"):
            self.load_test_manifest(manifest)

    def test_case_insensitive_destination_collision_is_rejected(self) -> None:
        manifest = make_manifest()
        manifest["documents"][1]["relative_path"] = "VENDOR/TEST_DOCUMENT.PDF"
        with self.assertRaisesRegex(ValueError, "duplicate document relative_path"):
            self.load_test_manifest(manifest)

    def test_relative_path_escape_is_rejected(self) -> None:
        for path in ("../test.pdf", "/test.pdf", "C:/test.pdf", "vendor\\test.pdf"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                downloader.validate_relative_path("test.document", path)

    def test_schema_one_is_not_silently_accepted(self) -> None:
        with self.assertRaisesRegex(ValueError, "schema_version"):
            self.load_test_manifest(dict(make_manifest(), schema_version=1))

    def test_list_modes_do_not_inspect_cache_or_connect(self) -> None:
        for arguments in (["--list-profiles"], ["--profile", "test_profile", "--list"]):
            with (
                self.subTest(arguments=arguments),
                mock.patch.object(sys, "argv", ["downloader", *arguments]),
                mock.patch.object(downloader, "load_manifest", return_value=make_manifest()),
                mock.patch.object(downloader, "inspect_documents", side_effect=AssertionError("cache access")),
                mock.patch.object(downloader.urllib.request, "urlopen", side_effect=AssertionError("network access")),
                contextlib.redirect_stdout(io.StringIO()),
            ):
                self.assertEqual(downloader.main(), 0)

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
