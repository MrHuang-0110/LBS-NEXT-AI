#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
LBS BootLoader 固件升级工具（Ymodem 发送端）

与 Boot/User/ymodem.c 配套：
  USB-1024  - STX 1KB 数据块（USB CDC 虚拟串口）
  BLUE-128  - SOH 128B 数据块（UART5 蓝牙，默认 115200 8N1）

固件下载（本脚本 Ymodem，逻辑不变）:
  python lbs_fw_update.py -p COM30 -f ..\\Output\\atk_e103.bin
  python lbs_fw_update.py -p COM7 --bt -f atk_e103.bin
  python lbs_fw_update.py -p COM3 -m USB-1024 -f app.bin -d

MDK 编译后弹窗（After Build 仍调用 mdk_deploy_prompt.py，内部转本脚本）:
  python mdk_deploy_prompt.py <固件.axf>

PikaPython 编译/脚本下发（委托 pika_deploy.py）:
  python lbs_fw_update.py compile main.py
  python lbs_fw_update.py deploy main.py -p COM7 --bt
  python lbs_fw_update.py pika main.py -p COM30 --compile-only

依赖: pip install pyserial
"""

from __future__ import annotations

import argparse
import os
import struct
import subprocess
import sys
import time
from pathlib import Path

try:
    import serial
except ImportError:
    print("请先安装 pyserial: pip install pyserial", file=sys.stderr)
    sys.exit(1)

SOH = 0x01
STX = 0x02
EOT = 0x04
ACK = 0x06
NAK = 0x15
CAN = 0x18
CRC_C = 0x43

HEADER_SIZE = 128
APP_MAX_SIZE = 0x08080000 - 0x08008000  # 与 boot_config.h 一致

# 与 APP app_tasks.c USB_CMD_FW_UPDATE 一致（拼写 fmware 勿改）
FW_UPDATE_CMD = b"ymodem update fmware\r\n"
PRE_UPGRADE_DELAY_S = 0.5
SERIAL_OPEN_RETRIES = 10
SERIAL_OPEN_INTERVAL_S = 3.0
USB_REOPEN_RETRIES = 40

TOOLS_DIR = Path(__file__).resolve().parent
PIKA_DEPLOY_SCRIPT = TOOLS_DIR / "pika_deploy.py"
DEPLOY_PORT_FILE = TOOLS_DIR / "deploy_port.txt"
DEPLOY_BT_PORT_FILE = TOOLS_DIR / "deploy_bt_port.txt"

MODES = {
    "USB-1024": {
        "data_mark": STX,
        "data_size": 1024,
        "byte_timeout": 0.05,
        "poll_min": 0.002,
        "ack_timeout": 12.0,
        "crc_wait": 120.0,
        "post_pkt_gap": 0.0,
    },
    "BLUE-128": {
        "data_mark": SOH,
        "data_size": 128,
        "byte_timeout": 0.3,
        "poll_min": 0.02,
        "ack_timeout": 30.0,
        "crc_wait": 120.0,
        "post_pkt_gap": 0.0,
    },
}


def crc16_xmodem(data: bytes) -> int:
    crc = 0
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def make_packet(mark: int, block_num: int, payload: bytes) -> bytes:
    blk = block_num & 0xFF
    crc = crc16_xmodem(payload)
    return bytes([mark, blk, (~blk) & 0xFF]) + payload + struct.pack(">H", crc)


class YmodemSender:
    def __init__(self, ser: serial.Serial, mode_cfg: dict):
        self.ser = ser
        self.data_mark = mode_cfg["data_mark"]
        self.data_size = mode_cfg["data_size"]
        self.byte_timeout = mode_cfg["byte_timeout"]
        self.poll_min = mode_cfg["poll_min"]
        self.ack_timeout = mode_cfg["ack_timeout"]
        self.crc_wait = mode_cfg["crc_wait"]
        self.post_pkt_gap = mode_cfg["post_pkt_gap"]

    def flush_input(self) -> None:
        self.ser.reset_input_buffer()

    def read_byte(self, timeout: float | None = None) -> int | None:
        old = self.ser.timeout
        self.ser.timeout = self.byte_timeout if timeout is None else timeout
        try:
            b = self.ser.read(1)
        finally:
            self.ser.timeout = old
        if not b:
            return None
        return b[0]

    def wait_byte(self, expected: int | set[int], timeout: float, desc: str) -> None:
        if isinstance(expected, int):
            expected = {expected}
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            remain = deadline - time.monotonic()
            if remain <= 0:
                break
            v = self.read_byte(timeout=max(self.poll_min, remain))
            if v is None:
                continue
            if v in expected:
                return
            if v == CAN:
                raise RuntimeError("设备取消传输 (CAN)")
        exp = "/".join(f"0x{x:02X}" for x in sorted(expected))
        raise TimeoutError(f"等待 {desc} 超时 (期望 {exp})")

    def wait_crc_request(self, timeout: float) -> None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            remain = deadline - time.monotonic()
            if remain <= 0:
                break
            v = self.read_byte(timeout=max(self.poll_min, remain))
            if v is None:
                continue
            if v == CRC_C:
                return
            if v == CAN:
                raise RuntimeError("设备取消传输 (CAN)")
        raise TimeoutError("等待设备发送 'C' 超时，请确认已进入 Boot 升级模式")

    def _usb_try_finish_early(self, usb_quick_exit: bool) -> bool:
        """USB 升级完成后设备会复位断线，读到 YMODEM OK 即结束，不再等待后续握手。"""
        if not usb_quick_exit:
            return False
        time.sleep(0.05)
        n = self.ser.in_waiting
        if not n:
            return False
        data = self.ser.read(n)
        if b"YMODEM OK" not in data:
            return False
        try:
            text = data.decode("utf-8", errors="replace").strip()
            if text:
                print(text)
        except Exception:
            pass
        return True

    def send(self, firmware: bytes, filename: str, *, usb_quick_exit: bool = False) -> None:
        size = len(firmware)
        if size == 0 or size > APP_MAX_SIZE:
            raise ValueError(f"固件大小无效: {size} (最大 {APP_MAX_SIZE})")

        header = filename.encode("ascii", errors="replace") + b"\x00" + str(size).encode("ascii") + b"\x00"
        header = header.ljust(HEADER_SIZE, b"\x00")[:HEADER_SIZE]
        pkt0 = make_packet(SOH, 0, header)

        print("等待 Boot 请求 (CRC 'C') ...")
        self.wait_crc_request(self.crc_wait)

        print("发送文件头 ...")
        self.ser.write(pkt0)
        self.ser.flush()
        self.wait_byte(ACK, self.ack_timeout, "ACK(文件头)")
        self.wait_crc_request(self.ack_timeout)

        block_num = 1
        offset = 0
        total = size
        while offset < total:
            chunk = firmware[offset : offset + self.data_size]
            chunk = chunk.ljust(self.data_size, b"\x1A")
            pkt = make_packet(self.data_mark, block_num, chunk)
            self.ser.write(pkt)
            self.ser.flush()
            if self.post_pkt_gap > 0:
                time.sleep(self.post_pkt_gap)
            self.wait_byte(ACK, self.ack_timeout, f"ACK(块 {block_num})")
            offset += self.data_size
            # 数据块号 1..255 循环；0 仅用于文件头/结束块（勿用 &0xFF，255 后会变成 0）
            block_num += 1
            if block_num > 255:
                block_num = 1
            pct = min(100, int(offset * 100 / total))
            print(f"\r传输进度: {pct:3d}% ({offset}/{total})", end="", flush=True)

        print("\r传输进度: 100% ({}/{})".format(total, total))

        try:
            print("结束传输 (EOT) ...")
            self.ser.write(b"\x04")
            self.ser.flush()
            self.wait_byte(NAK, self.ack_timeout, "NAK(EOT #1)")

            self.ser.write(b"\x04")
            self.ser.flush()
            self.wait_byte(ACK, self.ack_timeout, "ACK(EOT #2)")
            self.wait_crc_request(self.ack_timeout)

            end_payload = bytes(HEADER_SIZE)
            end_pkt = make_packet(SOH, 0, end_payload)
            self.ser.write(end_pkt)
            self.ser.flush()
            self.wait_byte(ACK, self.ack_timeout, "ACK(结束块)")
        except (TimeoutError, serial.SerialException, OSError):
            if usb_quick_exit:
                print("USB 固件已传完，设备复位断线，结束传输")
                return
            raise

        if self._usb_try_finish_early(usb_quick_exit):
            return

        if usb_quick_exit:
            return

        time.sleep(0.05)
        try:
            tail = self.ser.read(self.ser.in_waiting or 0)
        except (serial.SerialException, OSError):
            return
        if tail:
            try:
                text = tail.decode("utf-8", errors="replace").strip()
                if text:
                    print(text)
            except Exception:
                pass


def safe_close_serial(ser: serial.Serial | None) -> None:
    """关闭串口，设备复位导致端口消失时不重试、不抛错。"""
    if ser is None:
        return
    try:
        if ser.is_open:
            ser.close()
    except (serial.SerialException, OSError):
        pass


def open_serial(
    port: str,
    baud: int,
    byte_timeout: float,
    retries: int = SERIAL_OPEN_RETRIES,
    interval_s: float = SERIAL_OPEN_INTERVAL_S,
    label: str = "打开串口",
) -> serial.Serial:
    """打开串口，失败则按固定间隔重试。"""
    last_err: Exception | None = None
    for attempt in range(1, retries + 1):
        try:
            ser = serial.Serial(
                port=port,
                baudrate=baud,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=byte_timeout,
            )
            if attempt > 1:
                print(f"{label}成功: {port} (第 {attempt}/{retries} 次)")
            return ser
        except (serial.SerialException, OSError) as exc:
            last_err = exc
            if attempt < retries:
                print(
                    f"{label}失败 ({attempt}/{retries}): {port} — {exc}，"
                    f"{interval_s:.0f}s 后重试 ..."
                )
                time.sleep(interval_s)
    raise RuntimeError(
        f"{label}失败: 无法打开 {port}（已重试 {retries} 次，间隔 {interval_s:.0f}s）: {last_err}"
    ) from last_err


def drain_serial(ser: serial.Serial, idle_s: float = 0.15) -> bytes:
    """短暂等待并读走复位/回显残留。设备可能在此期间断开，异常安全。"""
    time.sleep(idle_s)
    try:
        n = ser.in_waiting
    except (serial.SerialException, OSError):
        return b""
    if not n:
        return b""
    try:
        return ser.read(n)
    except (serial.SerialException, OSError):
        return b""


def reopen_serial_after_reboot(
    port: str, baud: int, byte_timeout: float
) -> serial.Serial:
    """USB 复位后 COM 会消失，轮询直至主机重新枚举。"""
    return open_serial(
        port,
        baud,
        byte_timeout,
        retries=USB_REOPEN_RETRIES,
        interval_s=SERIAL_OPEN_INTERVAL_S,
        label="USB 重新枚举后打开串口",
    )


def prepare_device_for_upgrade(
    port: str, baud: int, mode: str, byte_timeout: float
) -> serial.Serial:
    """
    从 APP 触发进 Boot：先发 FW_UPDATE_CMD，固定等待 500ms。
    USB 模式再等待主机枚举；蓝牙模式保持原串口。
    """
    ser = open_serial(port, baud, byte_timeout)
    try:
        ser.reset_input_buffer()
        print("发送升级命令: ymodem update fmware")
        ser.write(FW_UPDATE_CMD)
        ser.flush()
        time.sleep(PRE_UPGRADE_DELAY_S)

        if mode == "USB-1024":
            # drain_serial 可能因设备复位断线而异常，不影响后续流程
            drain_serial(ser, idle_s=0.1)
            safe_close_serial(ser)
            print("等待 USB 重新枚举 ...")
            ser = reopen_serial_after_reboot(port, baud, byte_timeout)
            print("USB 枚举完成，串口已重新打开")
            return ser

        tail = drain_serial(ser, idle_s=0.2)
        if tail:
            try:
                text = tail.decode("utf-8", errors="replace").strip()
                if text:
                    print(text)
            except Exception:
                pass
        return ser
    except Exception:
        safe_close_serial(ser)
        raise


def load_deploy_port(*, bt: bool = False) -> str | None:
    """从 deploy_port.txt（USB）或 deploy_bt_port.txt（蓝牙）读取 COM。"""
    path = DEPLOY_BT_PORT_FILE if bt else DEPLOY_PORT_FILE
    if not path.is_file():
        return None
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            return line
    return None


def run_pika_deploy(argv: list[str]) -> int:
    """编译 / Pika 脚本下发等操作复用 pika_deploy.py。"""
    if not PIKA_DEPLOY_SCRIPT.is_file():
        print(f"未找到: {PIKA_DEPLOY_SCRIPT}", file=sys.stderr)
        return 1
    cmd = [sys.executable, "-u", str(PIKA_DEPLOY_SCRIPT), *argv]
    env = os.environ.copy()
    env["PYTHONUNBUFFERED"] = "1"
    print("执行:", " ".join(cmd), flush=True)
    return subprocess.call(cmd, env=env)


def parse_mode(value: str) -> str:
    key = value.strip().upper()
    alias = {
        "USB": "USB-1024",
        "USB1024": "USB-1024",
        "BT": "BLUE-128",
        "BLUE": "BLUE-128",
        "BT128": "BLUE-128",
    }
    key = alias.get(key, key)
    if key not in MODES:
        raise argparse.ArgumentTypeError(
            f"未知模式 '{value}'，可选: {', '.join(MODES)}"
        )
    return key


def main_compile(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="PikaPython 仅编译（委托 pika_deploy.py）")
    parser.add_argument("script", type=Path, help="Python 源文件 (.py)")
    parser.add_argument("-o", "--output", type=Path, help="输出 .py.o 路径")
    args = parser.parse_args(argv)
    pika_argv = [str(args.script), "--compile-only"]
    if args.output is not None:
        pika_argv.extend(["-o", str(args.output)])
    return run_pika_deploy(pika_argv)


def main_deploy(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="PikaPython 编译并下发（委托 pika_deploy.py）")
    parser.add_argument("script", type=Path, help="Python 源文件 (.py)")
    parser.add_argument("-p", "--port", help="串口/COM；省略时读 deploy_port.txt / deploy_bt_port.txt")
    parser.add_argument(
        "--bt",
        action="store_true",
        help="蓝牙模式（COM 默认见 deploy_bt_port.txt）",
    )
    parser.add_argument("-b", "--baud", type=int, default=None, help="上位机 COM 波特率")
    parser.add_argument(
        "--no-cmd",
        action="store_true",
        help="不发送 ymodem 命令（设备已在 YMODEM）",
    )
    args = parser.parse_args(argv)
    pika_argv = [str(args.script)]
    port = args.port or load_deploy_port(bt=args.bt)
    if port:
        pika_argv.extend(["-p", port])
    if args.bt:
        pika_argv.append("--bt")
    if args.baud is not None:
        pika_argv.extend(["-b", str(args.baud)])
    if args.no_cmd:
        pika_argv.append("--no-cmd")
    return run_pika_deploy(pika_argv)


def main_firmware(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="LBS BootLoader Ymodem 固件下载",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
模式说明:
  USB-1024   USB CDC，1KB (STX) 包，适合 USB 虚拟串口（默认，可省略 -m）
  BLUE-128   蓝牙 UART5，128B (SOH) 包（--bt 时默认）

默认从 APP 运行：自动发送「ymodem update fmware」、等待复位/USB 枚举后进 Boot 再传固件。
设备已在 Boot 升级界面：使用 -d / --download-only，仅 Ymodem 传文件，不发命令、不等枚举。

命令行示例:
  python lbs_fw_update.py -p COM30 -f ..\\Output\\atk_e103.bin
  python lbs_fw_update.py --bt -f atk_e103.bin
  python lbs_fw_update.py compile main.py
  python lbs_fw_update.py deploy main.py --bt
        """.strip(),
    )
    parser.add_argument(
        "-p", "--port", default=None, help="串口号；省略时读 tools/deploy_port.txt 或 --bt 时 deploy_bt_port.txt"
    )
    parser.add_argument(
        "-m",
        "--mode",
        default=None,
        type=parse_mode,
        help="传输模式: USB-1024 或 BLUE-128（--bt 时默认 BLUE-128，否则 USB-1024）",
    )
    parser.add_argument(
        "--bt",
        action="store_true",
        help="蓝牙固件升级（等同 -m BLUE-128，端口见 deploy_bt_port.txt）",
    )
    parser.add_argument(
        "-f", "--file", required=True, dest="firmware", help="固件 bin 文件路径"
    )
    parser.add_argument(
        "-b",
        "--baud",
        type=int,
        default=115200,
        help="串口波特率 (默认 115200)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=None,
        help="串口读字节超时(秒)；默认 USB=0.05、BLUE=0.3",
    )
    parser.add_argument(
        "-d",
        "--download-only",
        action="store_true",
        help="仅下载固件：不发升级命令、不等待 USB 枚举/复位（设备已在 Boot 升级模式）",
    )
    parser.add_argument(
        "--skip-trigger",
        action="store_true",
        help="同 --download-only（兼容旧参数）",
    )
    args = parser.parse_args(argv)
    download_only = args.download_only or args.skip_trigger

    mode = args.mode
    if mode is None:
        mode = "BLUE-128" if args.bt else "USB-1024"

    port = args.port or load_deploy_port(bt=args.bt)
    if not port:
        hint = "deploy_bt_port.txt" if args.bt else "deploy_port.txt"
        print(f"请指定 -p/--port，或在 tools/{hint} 中配置 COM", file=sys.stderr)
        return 1

    fw_path = os.path.abspath(args.firmware)
    if not os.path.isfile(fw_path):
        print(f"文件不存在: {fw_path}", file=sys.stderr)
        return 1

    with open(fw_path, "rb") as f:
        firmware = f.read()

    filename = os.path.basename(fw_path)
    mode_cfg = dict(MODES[mode])
    if args.timeout is not None:
        mode_cfg["byte_timeout"] = args.timeout

    print(f"端口: {port}")
    print(f"模式: {mode} (包 {mode_cfg['data_size']}B)")
    print(f"波特: {args.baud}")
    print(f"固件: {fw_path} ({len(firmware)} bytes)")
    if download_only:
        print("流程: 仅 Ymodem 下载（不发命令、不等待 USB 枚举）")

    try:
        if download_only:
            ser = open_serial(port, args.baud, mode_cfg["byte_timeout"])
        else:
            ser = prepare_device_for_upgrade(
                port, args.baud, mode, mode_cfg["byte_timeout"]
            )
    except (serial.SerialException, RuntimeError) as e:
        print(f"准备升级失败: {e}", file=sys.stderr)
        return 1

    usb_mode = mode == "USB-1024"
    rc = 0
    try:
        sender = YmodemSender(ser, mode_cfg)
        sender.flush_input()
        sender.send(firmware, filename, usb_quick_exit=usb_mode)
    except (TimeoutError, RuntimeError, ValueError) as e:
        print(f"\n失败: {e}", file=sys.stderr)
        rc = 1
    except KeyboardInterrupt:
        print("\n用户中断", file=sys.stderr)
        rc = 1
    finally:
        safe_close_serial(ser)

    if rc != 0:
        return rc

    print("升级完成")
    return 0


def main() -> int:
    argv = sys.argv[1:]
    if not argv:
        return main_firmware([])

    head = argv[0].lower()
    if head == "compile":
        return main_compile(argv[1:])
    if head == "deploy":
        return main_deploy(argv[1:])
    if head == "pika":
        return run_pika_deploy(argv[1:])
    if head in ("-h", "--help"):
        return main_firmware(argv)
    return main_firmware(argv)


if __name__ == "__main__":
    sys.exit(main())
