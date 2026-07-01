#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MDK µVision「编译后」调用：弹窗询问是否通过 USB 升级固件。

在 Keil Options -> User -> After Build 中配置（见 tools/固件更新.txt）。
端口：编辑 tools/deploy_port.txt 第一行，如 COM30
"""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
APP_ROOT = TOOLS.parent
FW_UPDATE_SCRIPT = TOOLS / "lbs_fw_update.py"
PORT_FILE = TOOLS / "deploy_port.txt"
DEFAULT_PORT = "COM30"


def load_port() -> str:
    if PORT_FILE.is_file():
        for line in PORT_FILE.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if line and not line.startswith("#"):
                return line
    return DEFAULT_PORT


def resolve_bin_path(arg: str) -> Path | None:
    p = Path(arg)
    if p.suffix.lower() == ".axf":
        p = p.with_suffix(".bin")
    if p.is_file():
        return p.resolve()
    return None


def ask_deploy(title: str, message: str) -> bool:
    if sys.platform != "win32":
        print(message)
        try:
            ans = input("是否升级? [y/N]: ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            return False
        return ans in ("y", "yes")

    import ctypes

    MB_YESNO = 0x04
    MB_ICONQUESTION = 0x20
    IDYES = 6
    r = ctypes.windll.user32.MessageBoxW(0, message, title, MB_YESNO | MB_ICONQUESTION)
    return r == IDYES


def main() -> int:
    if len(sys.argv) < 2:
        print("用法: mdk_deploy_prompt.py <固件.axf 或 .bin>", file=sys.stderr)
        return 0

    bin_path = resolve_bin_path(sys.argv[1])
    if bin_path is None:
        print(f"跳过 USB 升级: 未找到固件文件 ({sys.argv[1]})", file=sys.stderr)
        return 0

    port = load_port()
    msg = (
        f"固件: {bin_path.name}\n"
        f"大小: {bin_path.stat().st_size} bytes\n"
        f"串口: {port}\n\n"
        "是否现在通过 USB 升级到设备？\n"
        "（选「否」仅完成编译，不发送固件）"
    )
    if not ask_deploy("APP2 固件升级", msg):
        print("已跳过 USB 固件升级。")
        return 0

    cmd = [
        sys.executable,
        "-u",
        str(FW_UPDATE_SCRIPT),
        "-p",
        port,
        "-m",
        "USB-1024",
        "-f",
        str(bin_path),
    ]
    env = os.environ.copy()
    env["PYTHONUNBUFFERED"] = "1"

    # Keil Build Output 会缓存子进程 stdout，直到进程结束才一次性刷出；
    # 在独立控制台里跑部署脚本，进度才能实时显示。
    if sys.platform == "win32":
        print("已在独立命令行窗口启动固件升级，请查看实时进度。", flush=True)
        rc = subprocess.call(
            cmd,
            env=env,
            creationflags=subprocess.CREATE_NEW_CONSOLE,
        )
    else:
        print("执行:", " ".join(cmd), flush=True)
        rc = subprocess.call(cmd, env=env)

    if rc != 0:
        print(f"USB 升级失败 (exit={rc})", file=sys.stderr, flush=True)
    else:
        print("USB 固件升级完成。", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
