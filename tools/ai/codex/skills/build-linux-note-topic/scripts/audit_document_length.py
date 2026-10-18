#!/usr/bin/env python3
"""按汉字与英文词统计 Markdown；OVER 标记超过 10,000 后需要人工判断的文档。"""

import argparse
from pathlib import Path
import re


def count_units(content):
    """代码与图表文本计入，元数据、链接地址与 HTML 标记不计入。"""
    text = content.replace("\r\n", "\n")
    text = re.sub(r"\A---\n.*?\n---(?:\n|$)", "", text, count=1, flags=re.S)
    text = re.sub(r"\]\([^\n)]*\)", "]", text)
    text = re.sub(r"https?://[^\s<>]+", "", text)
    text = re.sub(r"</?(?:span|br|div|p|a|img|table|tr|td|th|details|summary)\b[^>]*>", "", text)
    return len(re.findall(
        r"[\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff]|[A-Za-z0-9_]+(?:[-.'][A-Za-z0-9_]+)*",
        text,
    ))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path)
    args = parser.parse_args()
    paths = set()
    for selected in args.paths:
        if not selected.exists():
            parser.error(f"路径不存在：{selected}")
        if selected.is_dir():
            paths.update(selected.rglob("*.md"))
        elif selected.suffix.lower() == ".md":
            paths.add(selected)
        else:
            parser.error(f"不是 Markdown 文件：{selected}")
    if not paths:
        parser.error("没有找到 Markdown 文件")
    exceeded = 0
    for path in sorted(paths):
        count = count_units(path.read_text(encoding="utf-8-sig"))
        over_limit = count > 10000
        exceeded += over_limit
        print(f"{'OVER' if over_limit else 'OK'} {count:>6} {path.as_posix()}")
    print(f"files={len(paths)} over_limit={exceeded} limit=10000; OVER requires a documented split/retain decision")
    return int(exceeded > 0)


if __name__ == "__main__":
    raise SystemExit(main())
