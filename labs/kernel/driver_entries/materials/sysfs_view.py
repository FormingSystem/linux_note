from pathlib import Path
import sys

def describe(entry):
    # sysfs 是活动视图：观察期间设备可能消失。
    try:
        location = entry.resolve(strict=True)
        driver = (entry / "driver").resolve(strict=True).name
    except FileNotFoundError:
        if not entry.exists():
            return f"{entry.name}: disappeared"
        location = entry.resolve()
        driver = "(no driver link)"
    return f"{entry.name}\n  object={location}\n  driver={driver}"

def main():
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("/sys/bus/platform/devices")
    if not root.is_dir():
        raise SystemExit(f"不是可读取的设备目录: {root}")
    entries = sorted(root.iterdir())[:5]
    if not entries:
        print("当前目录没有设备；这不表示内核中没有其他总线。")
    for entry in entries:
        print(describe(entry))

if __name__ == "__main__":
    main()
