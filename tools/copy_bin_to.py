#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
Keil After Build 脚本: 编译完成后将 .bin 文件拷贝到指定目录

用法:
  Keil After Build 中配置:
    python -u "..\..\tools\copy_bin_to.py" "$L@L.bin"

  目标目录配置在 tools/copy_bin_dest.txt 中, 一行一个路径, 支持 # 注释。
  例如:
    D:/MyFirmware/
    E:/Backup/

  目录不存在时自动创建。
"""

import os
import shutil
import sys
import time
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
CONFIG_FILE = TOOLS_DIR / "copy_bin_dest.txt"


def load_destinations(config_path: Path) -> list[Path]:
    """读取目标目录列表, 跳过空行和注释"""
    if not config_path.is_file():
        print(f"[copy_bin] 配置文件不存在: {config_path}", file=sys.stderr)
        print("[copy_bin] 请在 tools/copy_bin_dest.txt 中写入目标目录 (一行一个)", file=sys.stderr)
        return []

    destinations = []
    for line in config_path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        destinations.append(Path(line))
    return destinations


def copy_bin(src_path: Path, dest_dir: Path) -> bool:
    """拷贝 .bin 到目标目录, 自动创建目录, 同名文件直接覆盖"""
    try:
        dest_dir.mkdir(parents=True, exist_ok=True)
    except OSError as e:
        print(f"[copy_bin] 创建目录失败: {dest_dir} — {e}", file=sys.stderr)
        return False

    dest_file = dest_dir / src_path.name
    try:
        shutil.copy2(src_path, dest_file)
    except OSError as e:
        print(f"[copy_bin] 拷贝失败: {src_path} → {dest_file} — {e}", file=sys.stderr)
        return False

    size_kb = src_path.stat().st_size / 1024.0
    print(f"[copy_bin] {src_path.name} ({size_kb:.1f} KB) → {dest_file}")
    return True


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("用法: python copy_bin_to.py <bin文件路径>", file=sys.stderr)
        print("注: 此脚本供 Keil After Build 自动调用, 一般无需手动执行", file=sys.stderr)
        return 1

    src = Path(argv[1])
    if not src.is_file():
        print(f"[copy_bin] 源文件不存在: {src}", file=sys.stderr)
        return 1

    destinations = load_destinations(CONFIG_FILE)
    if not destinations:
        return 1

    timestamp = time.strftime("%H:%M:%S")
    print(f"[copy_bin] {timestamp} 编译完成, 开始拷贝...")

    ok = 0
    fail = 0
    for dest in destinations:
        if copy_bin(src, dest):
            ok += 1
        else:
            fail += 1

    print(f"[copy_bin] 完成: {ok} 成功, {fail} 失败")
    return 0 if fail == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))