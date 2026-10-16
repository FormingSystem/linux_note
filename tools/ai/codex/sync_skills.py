"""检查或将仓库权威 skill 单向同步到 Codex 安装镜像，不回写仓库。"""

import argparse
import json
import os
from pathlib import Path
import sys


def inventory(root):
    """忽略运行缓存，拒绝跟随链接，防止镜像越界。"""
    result = {}
    for path in sorted(root.rglob("*")):
        if "__pycache__" in path.parts or path.suffix in {".pyc", ".pyo"}:
            continue
        if path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction()):
            raise ValueError(f"不支持链接或联接：{path}")
        if path.is_file():
            result[path.relative_to(root)] = path.read_bytes()
    return result


def normalized(data):
    try:
        return data.decode("utf-8-sig").replace("\r\n", "\n").encode("utf-8")
    except UnicodeDecodeError:
        return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["check", "apply"])
    parser.add_argument("--codex-home", type=Path, default=Path(os.environ.get("CODEX_HOME", Path.home() / ".codex")))
    args = parser.parse_args()
    repo_root = Path(__file__).resolve().parents[3]
    registry = json.loads(Path(__file__).with_name("skill_registry.json").read_text(encoding="utf-8"))
    if registry["schema_version"] != 1:
        raise ValueError("不支持的注册表版本")
    changes = []
    snapshots = []
    for item in registry["skills"]:
        name = item["name"]
        if not name or Path(name).name != name or "/" in name or "\\" in name or name in {".", ".."}:
            raise ValueError("非法 skill 名称")
        if (item["direction"] != "repository_to_codex"
                or item["source"] != f"tools/ai/codex/skills/{name}"
                or item["mirror"] != f"codex_home/skills/{name}"):
            raise ValueError("注册关系必须从仓库权威副本指向 Codex 安装镜像")
        source = repo_root / item["source"]
        mirror = args.codex_home.absolute() / "skills" / name
        if (source.resolve() != source or mirror.resolve() != mirror
                or source.is_relative_to(mirror) or mirror.is_relative_to(source)):
            raise ValueError("仓库源与安装镜像必须是独立目录，不支持链接、联接或相互包含")
        if not (source / "SKILL.md").is_file():
            raise ValueError(f"仓库权威副本不可用：{name}；不会从安装镜像反向恢复")
        source_files = inventory(source)
        mirror_files = inventory(mirror) if mirror.exists() else {}
        if f"name: {name}" not in source_files[Path("SKILL.md")].decode("utf-8-sig").splitlines():
            raise ValueError(f"目录名与 skill name 不一致：{name}")
        extras = mirror_files.keys() - source_files.keys()
        if extras:
            raise ValueError(f"{name} 安装镜像存在仓库未收录的文件，先审查来源和调用关系：{sorted(map(str, extras))}")
        for relative, data in source_files.items():
            if relative not in mirror_files or normalized(data) != normalized(mirror_files[relative]):
                print(f"{'ADD' if relative not in mirror_files else 'UPDATE'} {name}/{relative.as_posix()}")
                changes.append((mirror / relative, data))
        snapshots.append((source, mirror, source_files))
    if args.mode == "apply":
        for path, data in changes:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        for source, mirror, snapshot in snapshots:
            if inventory(source) != snapshot:
                raise ValueError("同步期间仓库权威副本发生变化，请重新检查")
            current = inventory(mirror)
            if current.keys() != snapshot.keys() or any(normalized(data) != normalized(current[key]) for key, data in snapshot.items()):
                raise ValueError("同步后文件集或内容不一致")
    print(f"{args.mode}: skills={len(snapshots)} changes={len(changes)}")
    return int(args.mode == "check" and bool(changes))


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(2)
