"""验证提交标题、必填明细和段落格式。"""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class test_git_commit_message(unittest.TestCase):
    def check_message(self, message, accepted):
        shell = os.environ.get("GIT_HOOK_TEST_SHELL") or shutil.which("sh")
        if not shell:
            self.skipTest("需要 POSIX Shell 执行 Git 钩子")
        hook = Path(__file__).resolve().parents[1] / ".githooks" / "commit-msg"
        environment = os.environ.copy()
        environment["PATH"] = str(Path(shell).parent) + os.pathsep + environment.get("PATH", "")
        with tempfile.TemporaryDirectory() as directory:
            message_path = Path(directory) / "message.txt"
            message_path.write_text(message, encoding="utf-8", newline="\n")
            result = subprocess.run(
                [shell, str(hook), str(message_path)],
                capture_output=True,
                encoding="utf-8",
                env=environment,
            )
        self.assertNotIn("command not found", result.stderr)
        self.assertEqual(result.returncode == 0, accepted, result.stderr)

    def test_accepts_details(self):
        self.check_message("docs: 完善仓库规范\n\n- 要求提交包含修改明细\n", True)

    def test_accepts_scope_and_multiple_details(self):
        self.check_message("fix(repository/git)!: 严格校验提交\n\n- 拒绝缺少明细\n- 校验标题与明细分段\n", True)

    def test_accepts_chinese_scope_and_comments(self):
        self.check_message("docs(仓库/规范): 补齐约定\n\n# 模板说明\n- 记录校验规则\n", True)

    def test_rejects_title_only(self):
        self.check_message("docs: 完善仓库规范\n", False)

    def test_rejects_comment_only_body(self):
        self.check_message("docs: 完善仓库规范\n\n# - 请填写明细\n", False)

    def test_rejects_missing_separator(self):
        self.check_message("docs: 完善仓库规范\n- 要求修改明细\n", False)

    def test_rejects_empty_details(self):
        for detail in ("-", "- ", "-   ", "-\t", "- \t"):
            with self.subTest(detail=detail):
                self.check_message(f"docs: 完善仓库规范\n\n{detail}\n", False)

    def test_rejects_prose_body(self):
        self.check_message("docs: 完善仓库规范\n\n- 增加校验\n没有列表前缀\n", False)

    def test_preserves_title_constraints(self):
        for title in ("update: 修改规则", "docs: English only", "docs(a/b/c): 修改规则"):
            with self.subTest(title=title):
                self.check_message(f"{title}\n\n- 增加校验\n", False)


if __name__ == "__main__":
    unittest.main()
