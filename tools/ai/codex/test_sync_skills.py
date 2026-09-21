"""在临时仓库检查单向同步、失败边界及重复执行。"""

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


class sync_skills_test(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.repo = self.root / "repo"
        self.codex = self.root / "codex"
        self.script = self.repo / "tools/ai/codex/sync_skills.py"
        self.script.parent.mkdir(parents=True)
        shutil.copyfile(Path(__file__).with_name("sync_skills.py"), self.script)
        self.source = self.codex / "skills/example-skill"
        self.source.mkdir(parents=True)
        (self.source / "SKILL.md").write_text("---\nname: example-skill\ndescription: 测试同步\n---\n新版\n", encoding="utf-8")
        self.mirror = self.repo / "tools/ai/codex/skills/example-skill"
        self.registry = {"schema_version": 1, "skills": [{
            "name": "example-skill", "source": "codex_home/skills/example-skill",
            "mirror": "tools/ai/codex/skills/example-skill", "direction": "codex_to_repository",
        }]}
        self.write_registry()

    def write_registry(self):
        self.script.with_name("skill_registry.json").write_text(json.dumps(self.registry), encoding="utf-8")

    def run_sync(self, mode):
        return subprocess.run([sys.executable, str(self.script), mode, "--codex-home", str(self.codex)], capture_output=True)

    def test_check_is_read_only_and_apply_is_idempotent(self):
        before = (self.source / "SKILL.md").read_bytes()
        self.assertEqual(self.run_sync("check").returncode, 1)
        self.assertFalse(self.mirror.exists())
        self.assertEqual(self.run_sync("apply").returncode, 0)
        self.assertEqual((self.mirror / "SKILL.md").read_bytes(), before)
        self.assertEqual(self.run_sync("check").returncode, 0)
        self.assertEqual(self.run_sync("apply").returncode, 0)
        self.assertEqual((self.source / "SKILL.md").read_bytes(), before)

    def test_normalized_text_and_binary_assets(self):
        (self.source / "asset.bin").write_bytes(b"\xff\x00\r\n")
        self.assertEqual(self.run_sync("apply").returncode, 0)
        text = (self.source / "SKILL.md").read_text(encoding="utf-8")
        (self.mirror / "SKILL.md").write_bytes(b"\xef\xbb\xbf" + text.replace("\n", "\r\n").encode())
        self.assertEqual(self.run_sync("check").returncode, 0)
        self.assertEqual((self.mirror / "asset.bin").read_bytes(), b"\xff\x00\r\n")
        (self.source / "asset.bin").write_bytes(b"\xfe\x00")
        self.assertEqual(self.run_sync("check").returncode, 1)

    def test_cache_is_excluded(self):
        cache = self.source / "scripts/__pycache__"
        cache.mkdir(parents=True)
        (cache / "module.pyc").write_bytes(b"cache")
        self.assertEqual(self.run_sync("apply").returncode, 0)
        self.assertFalse((self.mirror / "scripts/__pycache__").exists())

    def test_missing_source_does_not_restore_from_mirror(self):
        self.assertEqual(self.run_sync("apply").returncode, 0)
        before = (self.mirror / "SKILL.md").read_bytes()
        (self.source / "SKILL.md").unlink()
        self.assertEqual(self.run_sync("apply").returncode, 2)
        self.assertFalse((self.source / "SKILL.md").exists())
        self.assertEqual((self.mirror / "SKILL.md").read_bytes(), before)

    def test_obsolete_file_blocks_all_writes(self):
        self.assertEqual(self.run_sync("apply").returncode, 0)
        before = (self.mirror / "SKILL.md").read_bytes()
        (self.mirror / "obsolete.md").write_text("待审查调用方", encoding="utf-8")
        (self.source / "SKILL.md").write_bytes(before + b"new change\n")
        self.assertEqual(self.run_sync("apply").returncode, 2)
        self.assertEqual((self.mirror / "SKILL.md").read_bytes(), before)
        self.assertTrue((self.mirror / "obsolete.md").exists())

    def test_invalid_destination_does_not_write(self):
        self.registry["skills"][0]["mirror"] = "../outside"
        self.write_registry()
        self.assertEqual(self.run_sync("apply").returncode, 2)
        self.assertFalse((self.root / "outside").exists())
        self.assertFalse(self.mirror.exists())


if __name__ == "__main__":
    unittest.main()
