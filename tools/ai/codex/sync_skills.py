"""按仓库注册表从 Codex 主源检查或同步 skill，不反向安装。"""

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
        if item["direction"] != "codex_to_repository" or item["source"] != f"codex_home/skills/{name}":
            raise ValueError("注册关系不是受支持的 Codex 主源")
        if not name or Path(name).name != name or "/" in name or "\\" in name or name in {".", ".."}:
            raise ValueError("非法 skill 名称")
        source = (args.codex_home / "skills" / name).resolve()
        mirror = repo_root / item["mirror"]
        expected = repo_root / "tools/ai/codex/skills" / name
        if mirror != expected or mirror.resolve() != expected.absolute() or source == mirror.resolve():
            raise ValueError("镜像路径必须是仓库内独立的 skill 目录")
        if not (source / "SKILL.md").is_file():
            raise ValueError(f"Codex 主源不可用：{name}；不会用仓库副本覆盖它")
        source_files = inventory(source)
        mirror_files = inventory(mirror) if mirror.exists() else {}
        if f"name: {name}" not in source_files[Path("SKILL.md")].decode("utf-8-sig").splitlines():
            raise ValueError(f"目录名与 skill name 不一致：{name}")
        extras = mirror_files.keys() - source_files.keys()
        if extras:
            raise ValueError(f"{name} 存在主源已无的文件，先审查调用关系并清理再同步：{sorted(map(str, extras))}")
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
                raise ValueError("同步期间主源发生变化，请重新检查")
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
