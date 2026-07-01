#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PikaPython 单文件编译下发，或 APP 固件 (.bin) Boot YMODEM 升级。

一步编译并下发（自动发 ymodem，无需手工先发命令）:
  python tools/pika_deploy.py my_app.py -p COM30          # USB
  python tools/pika_deploy.py my_app.py -p COM7 --bt      # 上位机串口接 BT 模块，默认 115200
  python tools/pika_deploy.py my_app.py --bt              # 端口见 deploy_bt_port.txt

仅编译、不下载:
  python tools/pika_deploy.py my_app.py --compile-only

固件升级 (Ymodem 由 tools/lbs_fw_update.py 执行，本脚本 --firmware 仅转调):
  python tools/lbs_fw_update.py -p COM3 -f Output/atk_e103.bin
  python tools/lbs_fw_update.py --bt -f Output/atk_e103.bin
  python tools/pika_deploy.py -p COM3 --firmware Output/atk_e103.bin   # 兼容，同上

依赖: pyserial  (pip install pyserial)
"""

from __future__ import annotations

import argparse
import os
import shutil
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

TOOLS_DIR = Path(__file__).resolve().parent
APP_ROOT = TOOLS_DIR.parent
SIBLING_APP_ROOT = APP_ROOT.parent / "APP"
COMPILER_NAME = "rust-msc-latest-win10.exe"
DEPLOY_PORT_FILE = TOOLS_DIR / "deploy_port.txt"
DEPLOY_BT_PORT_FILE = TOOLS_DIR / "deploy_bt_port.txt"


def _resolve_pika_root() -> Path:
    """Pika 工程根目录（编译器 cwd）；APP2 可能只有 python/ 而无 Middlewares/pikapython。"""
    candidates = (
        APP_ROOT / "Middlewares" / "pikapython",
        APP_ROOT / "python",
        SIBLING_APP_ROOT / "Middlewares" / "pikapython",
        TOOLS_DIR,
    )
    for path in candidates:
        if path.is_dir():
            return path
    return APP_ROOT / "python"


def _resolve_compiler() -> Path:
    """查找 rust-msc 预编译器；支持环境变量与兄弟 APP 工程。"""
    env = os.environ.get("PIKA_COMPILER", "").strip()
    if env:
        p = Path(env)
        if p.is_file():
            return p.resolve()

    candidates = (
        APP_ROOT / "Middlewares" / "pikapython" / COMPILER_NAME,
        TOOLS_DIR / COMPILER_NAME,
        SIBLING_APP_ROOT / "Middlewares" / "pikapython" / COMPILER_NAME,
    )
    for path in candidates:
        if path.is_file():
            return path.resolve()

    return candidates[0].resolve()


PIKA_ROOT = _resolve_pika_root()
COMPILER = _resolve_compiler()

_progress_last_pct: int = -1

SOH = 0x01
STX = 0x02
EOT = 0x04
ACK = 0x06
NAK = 0x15
CAN = 0x18
CRC_C = 0x43

USB_CMD_FW_UPDATE = b"ymodem update fmware\r\n"
# 发升级命令后等待 MCU 写 Flash、USB 断开（与 app_boot_param DRV_USB_DISCONNECT_MS 一致）
FW_PRE_UPGRADE_DELAY_S = 0.5
APP_MAX_FIRMWARE_SIZE = 0x08080000 - 0x08008000  # 与 Boot boot_config.h 一致
FW_CRC_WAIT_USB = 120.0
FW_CRC_WAIT_BT = 180.0
FW_ACK_TIMEOUT_USB = 12.0
FW_ACK_TIMEOUT_BT = 90.0
# 文件头：Boot 收首字节仅 YMODEM_POLL_MS(50)；握手后须立刻发 SOH。等 ACK 忽略周期 'C'。
FW_HEADER_ACK_TIMEOUT_USB = 10.0
FW_HEADER_ACK_TIMEOUT_BT = 8.0

# 与 Drivers/BSP/drv_bt_config.h 一致（ECB02 透传 pacing）
# 上位机 COM ↔ PC 侧蓝牙模块（与串口助手一致，常见 115200）
BT_HOST_BAUD_DEFAULT = 115200
# 设备 MCU UART5 ↔ ECB02 模块（见 drv_bt_config.h）
BT_DEVICE_BAUD = 115200
BT_YMODEM_BLOCK = 128
# ECB02：单帧 ≤248B 可全速透传；YMODEM SOH 包固定 133B，应一次写出
BT_ONESHOT_MAX = 248
BT_YMODEM_PKT_BYTES = 133
# 仅对 >248B 或超长 AT 命令才用 64B 分片（与 MCU drv_comm 一致）
BT_TX_CHUNK_MAX = 64
BT_TX_GAP_S = 0.03
# YMODEM 包间短间隔（勿用 30ms×分片，否则约 1 包/秒）
BT_FW_INTER_PACKET_S = 0.002
USB_BAUD_DEFAULT = 115200
USB_YMODEM_BLOCK = 1024

# APP 脚本下发收尾（显式 EOT 握手，对齐 drv_ymodem.c）
APP_FINISH_AFTER_OK_USB = 2.5
APP_FINISH_AFTER_OK_BT = 4.0
APP_FINISH_EOT_STEP_USB = 3.0
APP_FINISH_EOT_STEP_BT = 6.0
APP_DEPLOY_DRAIN_IDLE = 0.05
APP_DEPLOY_DRAIN_USB = 0.25
APP_DEPLOY_DRAIN_BT = 0.35
# 设备 YMODEM_SEND_C_MS=1000；文件头 ACK 后若未抓到 'C'，等待须 >1s（原 0.8s 会显示成「1s」超时）
APP_POST_HEADER_CRC_WAIT_USB = 3.5
APP_POST_HEADER_CRC_WAIT_BT = 4.0
# 发 ymodem 命令后给 MCU 主循环解析时间（HAL_Delay(5) 轮询）
APP_TRIGGER_POST_CMD_DELAY_USB = 0.12
APP_TRIGGER_PEEK_USB_S = 0.4


def _ensure_stdout_unbuffered() -> None:
    """MDK 编译输出等非 TTY 环境默认全缓冲，需强制行缓冲才能实时看到进度。"""
    if hasattr(sys.stdout, "reconfigure"):
        try:
            sys.stdout.reconfigure(line_buffering=True, write_through=True)
        except (ValueError, OSError):
            pass


def progress_reset() -> None:
    global _progress_last_pct
    _progress_last_pct = -1


def format_duration(seconds: float) -> str:
    """格式化为 分:秒 或 秒（带 1 位小数）。"""
    if seconds < 0:
        seconds = 0.0
    if seconds >= 60.0:
        m = int(seconds // 60)
        s = seconds - m * 60
        return f"{m}分{s:.1f}秒"
    return f"{seconds:.1f}秒"


def print_download_stats(
    link: str,
    size_bytes: int,
    total_s: float,
    ymodem_s: float | None = None,
) -> None:
    """打印固件下载耗时与平均速率。"""
    def kbps(elapsed: float) -> str:
        if elapsed <= 0:
            return "—"
        return f"{size_bytes / elapsed / 1024:.2f} KB/s"

    print(f"--- 下载统计 ({link}) ---", flush=True)
    print(f"  文件大小: {size_bytes} 字节 ({size_bytes / 1024:.1f} KB)", flush=True)
    if ymodem_s is not None:
        print(
            f"  YMODEM 传输: {format_duration(ymodem_s)}，"
            f"平均 {kbps(ymodem_s)}",
            flush=True,
        )
    print(
        f"  全程耗时:   {format_duration(total_s)}，"
        f"平均 {kbps(total_s)}",
        flush=True,
    )
    print("------------------------", flush=True)


def progress_report(done: int, total: int, *, prefix: str = "传输进度") -> None:
    """交互终端用 \\r；被重定向时用换行 + stderr（部分 IDE 对 stderr 缓冲更少）。"""
    global _progress_last_pct
    if total <= 0:
        return
    pct = min(100, int(done * 100 / total))
    if pct == _progress_last_pct:
        return
    _progress_last_pct = pct
    msg = f"{prefix}: {pct:3d}% ({done}/{total})"
    if sys.stdout.isatty():
        print(f"\r{msg}", end="", flush=True)
    else:
        print(msg, file=sys.stderr, flush=True)


def crc16_xmodem(data: bytes) -> int:
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def make_packet(seq: int, payload: bytes, block_size: int) -> bytes:
    if len(payload) > block_size:
        raise ValueError("payload too large")
    body = payload + bytes(block_size - len(payload))
    header = bytes([SOH if block_size == 128 else STX, seq & 0xFF, (~seq) & 0xFF])
    crc = crc16_xmodem(body)
    return header + body + struct.pack(">H", crc)


def serial_set_timeout(ser: serial.Serial, timeout: float) -> None:
    """COM 在设备复位后消失时，恢复 timeout 可能触发 configure port 错误，须忽略。"""
    try:
        ser.timeout = timeout
    except (serial.SerialException, OSError):
        pass


def serial_read(ser: serial.Serial, size: int) -> bytes:
    try:
        return ser.read(size)
    except (serial.SerialException, OSError):
        return b""


def is_serial_port_gone(exc: BaseException) -> bool:
    if isinstance(exc, OSError):
        winerr = getattr(exc, "winerror", None)
        if winerr in (22, 433, 2, 5):
            return True
    if isinstance(exc, serial.SerialException):
        msg = str(exc).lower()
        if "configure port" in msg or "not exist" in msg or "不存在" in str(exc):
            return True
        for sub in (getattr(exc, "__cause__", None), getattr(exc, "__context__", None)):
            if sub is not None and is_serial_port_gone(sub):
                return True
    return False


def serial_write(
    ser: serial.Serial,
    data: bytes,
    *,
    paced: bool = False,
    chunk_max: int = BT_TX_CHUNK_MAX,
    gap_s: float = BT_TX_GAP_S,
    packet_pace: bool = False,
) -> None:
    """蓝牙透传 pacing。

    ≤248B（含 YMODEM 133B）：一次 write，包间 BT_FW_INTER_PACKET_S。
    >248B：64B/30ms 分片（仅极少数长命令）。
    """
    if not data:
        return
    if not paced:
        try:
            ser.write(data)
            ser.flush()
        except (serial.SerialException, OSError) as exc:
            if is_serial_port_gone(exc):
                return
            raise
        return
    if packet_pace or len(data) <= BT_ONESHOT_MAX:
        ser.write(data)
        ser.flush()
        time.sleep(BT_FW_INTER_PACKET_S)
        return
    off = 0
    while off < len(data):
        n = min(chunk_max, len(data) - off)
        ser.write(data[off : off + n])
        ser.flush()
        off += n
        if off < len(data):
            time.sleep(gap_s)


def bt_read_all(
    ser: serial.Serial, overall: float, *, idle_gap: float = 0.25
) -> bytes:
    """阻塞读取；Windows 蓝牙 COM 的 in_waiting 常为 0，不能靠它判断有无数据。"""
    buf = bytearray()
    deadline = time.time() + overall
    old_timeout = ser.timeout
    serial_set_timeout(ser, min(0.3, idle_gap))
    last_rx = time.time()
    try:
        while time.time() < deadline:
            chunk = serial_read(ser, 4096)
            if chunk:
                buf.extend(chunk)
                last_rx = time.time()
            elif buf and (time.time() - last_rx) >= idle_gap:
                break
    finally:
        serial_set_timeout(ser, old_timeout)
    return bytes(buf)


def drain_serial_bytes(
    ser: serial.Serial, *, overall: float = 0.5, idle: float = 0.12
) -> bytes:
    buf = bytearray()
    capture_serial_tail(ser, buf, overall=overall, idle=idle)
    return bytes(buf)


def flush_serial_before_ymodem(ser: serial.Serial, *, bt: bool = False) -> None:
    """清空上位机 RX 里积压的监控 JSON，避免遮住 YMODEM 的 ACK/NAK/'C'。"""
    if not bt:
        try:
            ser.reset_input_buffer()
        except (serial.SerialException, OSError):
            pass
        return
    drain_serial_bytes(ser, overall=1.0, idle=0.08)


def ymodem_transfer_done_hint(data: bytes) -> bool:
    if not data:
        return False
    upper = data.upper()
    return b"YMODEM OK" in upper or b"YMODEM FAIL" in upper


def rx_has_crc_c(data: bytes) -> bool:
    """YMODEM CRC 握手仅认 0x43 ('C')；JSON 里大量小写 'c' 不能算握手。"""
    return CRC_C in data


def ymodem_ready_for_end_block(rx: bytes) -> bool:
    """EOT 双发后设备已 ACK（常见回显 NAK+ACK 或 ACK+'C'），须发空 SOH 结束块。"""
    if ymodem_transfer_done_hint(rx):
        return False
    if ACK not in rx:
        return False
    return (NAK in rx) or rx_has_crc_c(rx)


def ymodem_waiting_end_block(rx: bytes) -> bool:
    """同 ymodem_ready_for_end_block（兼容旧调用）。"""
    return ymodem_ready_for_end_block(rx)


def boot_ymodem_already_done(rx: bytes) -> bool:
    """Boot/APP 传输会话已结束（含 YMODEM OK 或多次 ACK 收尾）。"""
    if ymodem_transfer_done_hint(rx):
        return True
    if ymodem_ready_for_end_block(rx):
        return False
    if rx.count(ACK) >= 2:
        return True
    return False


def ymodem_session_done(rx: bytes) -> bool:
    return boot_ymodem_already_done(rx) or ymodem_transfer_done_hint(rx)


def poll_until_ymodem_done(
    ser: serial.Serial,
    rx_acc: bytearray,
    timeout: float,
    *,
    bt: bool = False,
) -> bool:
    """短轮询直到 YMODEM OK/FAIL 或超时；不因监控 JSON 持续上报而拖满 absorb。"""
    deadline = time.time() + timeout
    while time.time() < deadline:
        remain = deadline - time.time()
        if remain <= 0.0:
            break
        chunk = poll_rx_window(ser, min(0.1, remain), bt=bt)
        if chunk:
            rx_acc.extend(chunk)
        if ymodem_session_done(bytes(rx_acc)):
            return True
    return ymodem_session_done(bytes(rx_acc))


def poll_rx_window(
    ser: serial.Serial, seconds: float, *, bt: bool = False
) -> bytes:
    if bt:
        idle_gap = 0.12 if seconds <= 0.5 else 0.25
        return bt_read_all(ser, seconds, idle_gap=idle_gap)
    idle = 0.05 if seconds <= 0.5 else 0.12
    return drain_serial_bytes(ser, overall=seconds, idle=idle)


def brief_drain_after_deploy(ser: serial.Serial, *, bt: bool = False) -> None:
    """USB/BT 下发结束后短暂收尾；避免监控 JSON 导致长 drain。"""
    drain_serial(
        ser,
        idle_timeout=APP_DEPLOY_DRAIN_IDLE,
        overall=APP_DEPLOY_DRAIN_BT if bt else APP_DEPLOY_DRAIN_USB,
    )


def _ymodem_skip_rx_byte(b: int) -> bool:
    """跳过监控 JSON 等可打印字符，只在 YMODEM 控制字节上响应。"""
    if b in (ACK, NAK, CAN, CRC_C, SOH, STX, EOT):
        return False
    return 0x20 <= b < 0x7F


def wait_byte(
    ser: serial.Serial, expected: set[int], timeout: float, *, bt: bool = False
) -> int:
    deadline = time.time() + timeout
    old_timeout = ser.timeout
    serial_set_timeout(ser, 0.05)
    recent = bytearray()
    read_sz = 256 if bt else 64
    try:
        while time.time() < deadline:
            chunk = serial_read(ser, read_sz)
            if not chunk:
                continue
            for b in chunk:
                if _ymodem_skip_rx_byte(b):
                    continue
                if len(recent) < 160:
                    recent.append(b)
                else:
                    recent.pop(0)
                    recent.append(b)
                if b in expected:
                    return b
                if b == CAN:
                    raise RuntimeError("设备取消传输 (CAN)")
    finally:
        serial_set_timeout(ser, old_timeout)
    tail = recent[-80:].decode("utf-8", errors="replace")
    raise TimeoutError(
        f"等待 {expected!r} 超时；近期 {len(recent)} 字节 "
        f"(C={recent.count(CRC_C)}, ACK={recent.count(ACK)}, NAK={recent.count(NAK)})；"
        f"尾部: {tail!r}"
    )


def wait_byte_ignore_crc_c(
    ser: serial.Serial,
    expected: set[int],
    timeout: float,
    *,
    bt: bool = False,
    rx_tail: bytearray | None = None,
) -> int:
    """等 ACK/NAK，忽略周期性 'C'；保留 C 与 ACK 同包后续字节供下一握手。"""
    deadline = time.time() + timeout
    old_timeout = ser.timeout
    serial_set_timeout(ser, 0.05)
    recent = bytearray()
    read_sz = 256 if bt else 64
    try:
        while time.time() < deadline:
            chunk = serial_read(ser, read_sz)
            if not chunk:
                continue
            for i, b in enumerate(chunk):
                if _ymodem_skip_rx_byte(b) and b != CRC_C:
                    continue
                if len(recent) < 160:
                    recent.append(b)
                else:
                    recent.pop(0)
                    recent.append(b)
                if b == CRC_C:
                    if rx_tail is not None:
                        rx_tail.append(b)
                    continue
                if b in expected:
                    if rx_tail is not None:
                        rx_tail.extend(chunk[i + 1 :])
                    return b
                if b == CAN:
                    raise RuntimeError("设备取消传输 (CAN)")
    finally:
        serial_set_timeout(ser, old_timeout)
    tail = recent[-80:].decode("utf-8", errors="replace")
    hint = ""
    if b"deviceList" in recent or b'"adc"' in recent:
        hint = "（串口积压了设备监控 JSON，请更新 pika_deploy 或重新运行以先清空缓冲）"
    raise TimeoutError(
        f"等待 {expected!r} 超时（已忽略周期性 'C'）；近期 {len(recent)} 字节 "
        f"(ACK={recent.count(ACK)}, NAK={recent.count(NAK)})；尾部: {tail!r}{hint}"
    )


def capture_serial_tail(
    ser: serial.Serial, buf: bytearray, *, overall: float = 0.6, idle: float = 0.12
) -> None:
    """ACK 后把串口余量读入 buf（蓝牙常把 ACK 与 'C' 粘在同一包）。"""
    deadline = time.time() + overall
    last_rx = time.time()
    old_timeout = ser.timeout
    serial_set_timeout(ser, 0.08)
    try:
        while time.time() < deadline:
            chunk = serial_read(ser, 256)
            if chunk:
                buf.extend(chunk)
                last_rx = time.time()
            elif time.time() - last_rx >= idle:
                break
    finally:
        serial_set_timeout(ser, old_timeout)


def send_pkt_wait_ack(
    ser: serial.Serial,
    pkt: bytes,
    *,
    paced: bool,
    ack_timeout: float,
    label: str = "",
    max_retries: int = 10,
    rx_tail: bytearray | None = None,
    packet_pace: bool = False,
    ignore_crc_c: bool = False,
) -> None:
    """发送一包并等 ACK；NAK/CAN/超时则重发（设备持续发 C 表示未收到有效包）。"""
    tag = label or "YMODEM 块"
    retries = max_retries if paced else 3
    for attempt in range(1, retries + 1):
        if not paced:
            try:
                ser.reset_input_buffer()
            except (serial.SerialException, OSError):
                pass
        serial_write(ser, pkt, paced=paced, packet_pace=packet_pace)
        if tag:
            print(f"{tag} 等待 ACK ({attempt}/{retries})...", flush=True)
        wait_fn = wait_byte_ignore_crc_c if ignore_crc_c else wait_byte
        try:
            if ignore_crc_c:
                resp = wait_fn(
                    ser,
                    {ACK, NAK},
                    ack_timeout,
                    bt=paced,
                    rx_tail=rx_tail,
                )
            else:
                resp = wait_fn(ser, {ACK, NAK}, ack_timeout, bt=paced)
        except TimeoutError:
            if ignore_crc_c:
                extra = drain_serial_bytes(
                    ser, overall=1.5 if paced else 0.6, idle=0.12
                )
                if ymodem_transfer_done_hint(extra):
                    if rx_tail is not None:
                        rx_tail.extend(extra)
                    return
            if attempt < retries:
                continue
            raise
        except RuntimeError as exc:
            if "CAN" in str(exc) and attempt < retries:
                continue
            raise
        if resp == ACK:
            if rx_tail is not None:
                capture_serial_tail(ser, rx_tail)
            if tag:
                print(f"{tag} 已确认。", flush=True)
            return
        if resp == NAK and attempt < retries:
            print(f"{tag} 收到 NAK，重发...", flush=True)
            continue
    raise TimeoutError(f"{tag} 多次重试后设备仍无 ACK")


def ymodem_finish_app_session(
    ser: serial.Serial,
    *,
    paced: bool,
    ack_timeout: float,
    packet_pace: bool = False,
) -> None:
    """EOT 双发 + 空块 + 等待 YMODEM OK，与 APP Middlewares/ymodem/drv_ymodem.c 一致。"""
    rx_acc = bytearray()
    end_mark = bytes([0x04])
    end_pace = packet_pace or (paced and len(end_mark) <= BT_ONESHOT_MAX)
    step_timeout = (
        APP_FINISH_EOT_STEP_BT if paced else APP_FINISH_EOT_STEP_USB
    )
    step_timeout = min(step_timeout, ack_timeout)
    ok_wait = APP_FINISH_AFTER_OK_BT if paced else APP_FINISH_AFTER_OK_USB

    serial_write(ser, end_mark, paced=paced, packet_pace=end_pace)
    try:
        wait_byte(ser, {NAK}, step_timeout, bt=paced)
    except TimeoutError:
        chunk = drain_serial_bytes(ser, overall=0.35, idle=0.06)
        if chunk:
            rx_acc.extend(chunk)
        if NAK not in rx_acc and not ymodem_transfer_done_hint(rx_acc):
            raise

    serial_write(ser, end_mark, paced=paced, packet_pace=end_pace)
    eot_tail = bytearray(rx_acc)
    try:
        wait_byte_ignore_crc_c(
            ser, {ACK}, step_timeout, bt=paced, rx_tail=eot_tail
        )
    except TimeoutError:
        extra = drain_serial_bytes(
            ser, overall=0.8 if paced else 0.45, idle=0.08
        )
        eot_tail.extend(extra)
        if ACK not in eot_tail and not ymodem_transfer_done_hint(eot_tail):
            raise
    rx_acc = bytearray(eot_tail)

    end_pkt = make_packet(0, b"", 128)
    end_tail = bytearray()
    send_pkt_wait_ack(
        ser,
        end_pkt,
        paced=paced,
        ack_timeout=step_timeout,
        label="结束块 (block 0)",
        max_retries=5,
        packet_pace=packet_pace or (paced and len(end_pkt) <= BT_ONESHOT_MAX),
        ignore_crc_c=True,
        rx_tail=end_tail,
    )
    rx_acc.extend(end_tail)

    if not poll_until_ymodem_done(ser, rx_acc, ok_wait, bt=paced):
        rx_acc.extend(
            drain_serial_bytes(
                ser,
                overall=APP_DEPLOY_DRAIN_BT if paced else APP_DEPLOY_DRAIN_USB,
                idle=0.08,
            )
        )
    if ymodem_session_done(bytes(rx_acc)):
        return
    if paced:
        return

    raw_tail = bytes(rx_acc)[-24:]
    text_tail = bytes(rx_acc)[-120:].decode("utf-8", errors="replace")
    raise TimeoutError(
        f"YMODEM 收尾超时；近期回显: {text_tail!r} (hex: {raw_tail.hex(' ')})。"
        "脚本下发请确认 APP 已烧录最新 drv_ymodem.c。"
    )


def open_serial(port: str, baud: int) -> serial.Serial:
    """USB CDC: 关闭 DTR/RTS 避免 MCU 复位。"""
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = baud
    ser.timeout = 0.05
    ser.write_timeout = 5.0
    ser.dsrdtr = False
    ser.rtscts = False
    ser.open()
    ser.dtr = False
    ser.rts = False
    time.sleep(0.15)
    ser.reset_input_buffer()
    return ser


def open_bt_serial(port: str, baud: int) -> serial.Serial:
    """蓝牙转串口：参数尽量与串口助手一致，不拉 DTR/不设 exclusive。"""
    ser = serial.Serial(
        port=port,
        baudrate=baud,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=1.0,
        write_timeout=10.0,
        xonxoff=False,
        rtscts=False,
        dsrdtr=False,
    )
    return ser


def send_bt_line(ser: serial.Serial, line: str) -> None:
    """与串口助手一致：一行命令 + CRLF 一次发出。"""
    ser.write(line.encode("ascii") + b"\r\n")
    ser.flush()


def bt_probe_link(ser: serial.Serial, *, overall: float = 1.0) -> bool:
    """发 mac 探测双向链路（固件蓝牙命令）。"""
    send_bt_line(ser, "mac")
    resp = bt_read_all(ser, overall, idle_gap=0.12)
    return bool(resp)


def drain_serial(
    ser: serial.Serial, idle_timeout: float = 0.3, overall: float = 5.0
) -> str:
    buf = bytearray()
    deadline = time.time() + overall
    last_rx = time.time()
    old_timeout = ser.timeout
    serial_set_timeout(ser, 0.05)
    try:
        while time.time() < deadline:
            chunk = serial_read(ser, 256)
            if chunk:
                buf.extend(chunk)
                last_rx = time.time()
            elif time.time() - last_rx >= idle_timeout:
                break
    finally:
        serial_set_timeout(ser, old_timeout)
    return buf.decode("utf-8", errors="replace")


def safe_close_serial(ser: serial.Serial, *, bt: bool = False) -> None:
    if not ser.is_open:
        return
    if not bt:
        try:
            ser.dtr = False
            ser.rts = False
        except (serial.SerialException, OSError):
            pass
    if hasattr(ser, "cancel_read"):
        try:
            ser.cancel_read()
        except (serial.SerialException, OSError):
            pass
    try:
        ser.reset_input_buffer()
        ser.reset_output_buffer()
    except (serial.SerialException, OSError):
        pass
    try:
        ser.close()
    except (serial.SerialException, OSError) as exc:
        if not is_serial_port_gone(exc):
            print(f"关闭串口异常（可忽略）: {exc}", file=sys.stderr)


def wait_crc_request(
    ser: serial.Serial,
    timeout: float = 120.0,
    *,
    bt: bool = False,
    prefetch: bytes = b"",
) -> None:
    """等待 YMODEM CRC 握手字节 'C' (0x43)。

    prefetch: 调用方已读入的字节（如蓝牙 trigger 时的首包回显），须在此一并扫描，
    否则 'C' 已被 bt_read_all 消费会导致永远等不到。
    """
    deadline = time.time() + timeout
    discarded = bytearray()
    old_timeout = ser.timeout
    serial_set_timeout(ser, 0.12 if bt else 0.02)

    def consume(chunk: bytes) -> bool:
        for b in chunk:
            if len(discarded) < 2048:
                discarded.append(b)
            if b == CRC_C:
                if boot_ymodem_already_done(bytes(discarded)):
                    return True
                return True
        if boot_ymodem_already_done(bytes(discarded)):
            return True
        return False

    try:
        if prefetch and consume(prefetch):
            return
        while time.time() < deadline:
            try:
                n = 256 if bt else (ser.in_waiting or 1)
            except (serial.SerialException, OSError):
                n = 1
            chunk = serial_read(ser, n)
            if not chunk:
                if not bt:
                    time.sleep(0.003)
                continue
            if consume(chunk):
                return
    finally:
        serial_set_timeout(ser, old_timeout)
    hint = discarded[:120].decode("utf-8", errors="replace")
    extra = ""
    if bt and len(discarded) == 0:
        extra = "（无回包：关闭其它串口助手；确认已烧录最新固件）"
    raise TimeoutError(
        f"未在 {timeout:.1f}s 内收到 'C'，已收到: {hint!r}。{extra}"
        "请确认设备已进入 YMODEM 模式，且 COM 口未被占用。"
    )


def trigger_device_ymodem(
    ser: serial.Serial, *, bt: bool = False, probe_bt: bool = False
) -> None:
    print("清空串口缓冲...", flush=True)
    flush_serial_before_ymodem(ser, bt=bt)
    peek = b""
    if bt:
        if probe_bt:
            bt_probe_link(ser, overall=0.5)
        print("发送 ymodem 命令 (蓝牙)...", flush=True)
        send_bt_line(ser, "ymodem")
        peek = bt_read_all(ser, 0.25, idle_gap=0.05)
    else:
        print("发送 ymodem 命令...", flush=True)
        ser.write(b"ymodem\r\n")
        ser.flush()
        time.sleep(APP_TRIGGER_POST_CMD_DELAY_USB)
    print("等待设备 YMODEM 握手 (0x43 'C')...", flush=True)
    wait_crc_request(
        ser, timeout=30.0 if bt else 15.0, bt=bt, prefetch=peek
    )
    flush_serial_before_ymodem(ser, bt=bt)
    print("设备已进入 YMODEM 模式。", flush=True)


def trigger_firmware_reboot(ser: serial.Serial) -> None:
    print("发送固件升级命令...", flush=True)
    ser.write(USB_CMD_FW_UPDATE)
    ser.flush()
    print(
        f"等待 {FW_PRE_UPGRADE_DELAY_S:.1f}s（设备复位并断开 USB）...",
        flush=True,
    )
    time.sleep(FW_PRE_UPGRADE_DELAY_S)
    drain_serial(ser, idle_timeout=0.2, overall=3.0)


def trigger_firmware_reboot_bt(ser: serial.Serial) -> None:
    """与 USB 完全相同字节流；经蓝牙透传需分片发送。"""
    try:
        ser.reset_input_buffer()
    except OSError:
        pass
    print("发送固件升级命令 (蓝牙)...", flush=True)
    serial_write(ser, USB_CMD_FW_UPDATE, paced=True)
    time.sleep(FW_PRE_UPGRADE_DELAY_S)
    bt_read_all(ser, 1.0, idle_gap=0.1)


def reopen_serial_after_reboot(
    port: str, baud: int, retries: int = 60, interval: float = 0.5
) -> serial.Serial:
    """USB 复位后 COM 会短暂消失，需轮询直至主机完成重新枚举。"""
    last_err: Exception | None = None
    for attempt in range(1, retries + 1):
        try:
            return open_serial(port, baud)
        except (serial.SerialException, OSError) as exc:
            last_err = exc
            time.sleep(interval)
    raise RuntimeError(f"复位后无法打开 {port}: {last_err}") from last_err


def ymodem_send(
    ser: serial.Serial,
    file_path: Path,
    *,
    block_size: int = USB_YMODEM_BLOCK,
    paced: bool = False,
    ack_timeout: float | None = None,
    packet_pace: bool = False,
    await_initial_c: bool = False,
    initial_prefetch: bytes = b"",
) -> None:
    data = file_path.read_bytes()
    name = file_path.name.encode("ascii", errors="replace")
    size_str = str(len(data)).encode("ascii")
    header = name + b"\x00" + size_str + b"\x00"
    if len(header) > 128:
        raise ValueError("文件名过长")
    if block_size not in (128, 1024):
        raise ValueError("block_size 须为 128 (蓝牙) 或 1024 (USB)")

    if ack_timeout is None:
        ack_timeout = FW_ACK_TIMEOUT_BT if paced else FW_ACK_TIMEOUT_USB
    header_ack_timeout = (
        FW_HEADER_ACK_TIMEOUT_BT if paced else FW_HEADER_ACK_TIMEOUT_USB
    )
    total = len(data)
    if not await_initial_c:
        flush_serial_before_ymodem(ser, bt=paced)

    if await_initial_c:
        wait_crc_request(
            ser,
            timeout=FW_CRC_WAIT_BT if paced else FW_CRC_WAIT_USB,
            bt=paced,
            prefetch=initial_prefetch,
        )

    pkt = make_packet(0, header, 128)
    tail_after_ack = bytearray()
    print("发送 YMODEM 文件头...", flush=True)
    # 收到 'C' 后立即发 SOH；等 ACK 时保留同包内的下一字节 'C'（USB/BT 共用）
    send_pkt_wait_ack(
        ser,
        pkt,
        paced=paced,
        ack_timeout=header_ack_timeout,
        label="文件头 (block 0)",
        max_retries=3,
        rx_tail=tail_after_ack,
        packet_pace=packet_pace or (paced and len(pkt) <= BT_ONESHOT_MAX),
        ignore_crc_c=True,
    )
    if not rx_has_crc_c(tail_after_ack):
        wait_crc_request(
            ser,
            timeout=APP_POST_HEADER_CRC_WAIT_BT
            if paced
            else APP_POST_HEADER_CRC_WAIT_USB,
            bt=paced,
            prefetch=bytes(tail_after_ack),
        )

    seq = 1
    offset = 0
    progress_reset()
    while offset < total:
        chunk = data[offset : offset + block_size]
        pkt = make_packet(seq, chunk, block_size)
        send_pkt_wait_ack(
            ser,
            pkt,
            paced=paced,
            ack_timeout=ack_timeout,
            label=f"数据块 #{seq}",
            max_retries=8,
            packet_pace=packet_pace or (paced and len(pkt) <= BT_ONESHOT_MAX),
        )
        offset += block_size
        # YMODEM 数据块号为 1..255 循环，0 仅用于文件头/结束块；勿用 &0xFF（255→0）
        seq += 1
        if seq > 255:
            seq = 1
        prefix = "蓝牙传输" if paced else "传输进度"
        progress_report(min(offset, total), total, prefix=prefix)

    progress_report(total, total, prefix="蓝牙传输" if paced else "传输进度")
    if sys.stdout.isatty():
        print(flush=True)

    ymodem_finish_app_session(
        ser,
        paced=paced,
        ack_timeout=ack_timeout,
        packet_pace=packet_pace,
    )


def deploy_firmware(port: str, baud: int, bin_path: Path, *, bt: bool = False) -> None:
    link = "蓝牙" if bt else "USB"
    size_bytes = bin_path.stat().st_size
    t_total_start = time.monotonic()
    print(f"固件 ({link}): {bin_path.resolve()} ({size_bytes} bytes)")

    if bt:
        ser = open_bt_serial(port, baud)
        t_ymodem_s: float | None = None
        try:
            trigger_firmware_reboot_bt(ser)
            time.sleep(2.0)
            prefetch = bt_read_all(ser, 3.0)
            t_ymodem_start = time.monotonic()
            ymodem_send(
                ser,
                bin_path,
                block_size=BT_YMODEM_BLOCK,
                paced=True,
                packet_pace=True,
                ack_timeout=FW_ACK_TIMEOUT_BT,
                await_initial_c=True,
                initial_prefetch=prefetch,
            )
            t_ymodem_s = time.monotonic() - t_ymodem_start
            brief_drain_after_deploy(ser, bt=True)
        except (serial.SerialException, OSError) as exc:
            print(f"蓝牙固件升级串口异常: {exc}", file=sys.stderr)
            raise
        finally:
            safe_close_serial(ser, bt=True)
        print("固件传输完成。")
        print_download_stats(
            link, size_bytes, time.monotonic() - t_total_start, t_ymodem_s
        )
        return

    ser = open_serial(port, baud)
    t_ymodem_s = None
    try:
        trigger_firmware_reboot(ser)
    except (serial.SerialException, OSError):
        pass
    finally:
        safe_close_serial(ser)

    print("等待 USB 重新枚举...", flush=True)
    ser = reopen_serial_after_reboot(port, baud)
    xfer_ok = False
    try:
        t_ymodem_start = time.monotonic()
        ymodem_send(
            ser,
            bin_path,
            block_size=USB_YMODEM_BLOCK,
            paced=False,
            ack_timeout=FW_ACK_TIMEOUT_USB,
            await_initial_c=True,
        )
        t_ymodem_s = time.monotonic() - t_ymodem_start
        xfer_ok = True
        try:
            brief_drain_after_deploy(ser, bt=False)
        except (serial.SerialException, OSError) as exc:
            if not is_serial_port_gone(exc):
                raise
    finally:
        safe_close_serial(ser)
    print("固件传输完成。")
    if xfer_ok:
        print_download_stats(link, size_bytes, time.monotonic() - t_total_start, t_ymodem_s)


def default_obj_path(py_file: Path) -> Path:
    return py_file.with_suffix(".py.o")


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


def run_lbs_fw_update(port: str, baud: int, bin_path: Path, *, bt: bool = False) -> int:
    """固件 YMODEM 下载委托 tools/lbs_fw_update.py（与 Boot ymodem.c 配套）。"""
    lbs_script = TOOLS_DIR / "lbs_fw_update.py"
    mode = "BLUE-128" if bt else "USB-1024"
    cmd = [
        sys.executable,
        "-u",
        str(lbs_script),
        "-p",
        port,
        "-m",
        mode,
        "-f",
        str(bin_path),
        "-b",
        str(baud),
    ]
    env = os.environ.copy()
    env["PYTHONUNBUFFERED"] = "1"
    return subprocess.call(cmd, env=env)


def compile_py_to_o(py_file: Path, obj_out: Path | None = None) -> Path:
    compiler = _resolve_compiler()
    if not compiler.is_file():
        raise FileNotFoundError(
            "找不到 PikaPython 预编译器 rust-msc-latest-win10.exe。\n"
            "请任选其一：\n"
            f"  1) 复制到 {TOOLS_DIR / COMPILER_NAME}\n"
            f"  2) 复制到 {APP_ROOT / 'Middlewares' / 'pikapython' / COMPILER_NAME}\n"
            f"  3) 设置环境变量 PIKA_COMPILER=完整路径\n"
            f"  4) 从兄弟工程 APP 复制: "
            f"{SIBLING_APP_ROOT / 'Middlewares' / 'pikapython' / COMPILER_NAME}\n"
            "也可在 APP/Middlewares/pikapython 下运行 pikaPackage.exe 拉取。"
        )
    if not py_file.is_file():
        raise FileNotFoundError(py_file)

    py_abs = py_file.resolve()
    out_abs = (obj_out if obj_out else default_obj_path(py_abs)).resolve()
    out_abs.parent.mkdir(parents=True, exist_ok=True)

    pika_root = _resolve_pika_root()
    cmd = [str(compiler), "-c", str(py_abs), "-o", str(out_abs)]
    print(f"编译: {' '.join(cmd)}")
    proc = subprocess.run(
        cmd,
        cwd=str(pika_root),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if proc.stdout:
        print(proc.stdout.strip())
    if proc.stderr:
        print(proc.stderr.strip(), file=sys.stderr)
    if proc.returncode != 0:
        raise RuntimeError(f"编译失败, exit={proc.returncode}")
    if not out_abs.is_file():
        raise FileNotFoundError(f"未生成 {out_abs}")

    print(f"生成: {out_abs} ({out_abs.stat().st_size} bytes)")
    return out_abs


def main() -> int:
    _ensure_stdout_unbuffered()
    parser = argparse.ArgumentParser(
        description="PikaPython 编译下发，或 .bin 固件 Boot YMODEM 升级",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "一步下发:  python tools/pika_deploy.py app.py -p COM30\n"
            "蓝牙一步:  python tools/pika_deploy.py app.py --bt  (COM 见 deploy_bt_port.txt)\n"
            "固件 USB: python tools/lbs_fw_update.py -p COM30 -f ..\\Output\\atk_e103.bin\n"
            "固件 BT:  python tools/lbs_fw_update.py --bt -f ..\\Output\\atk_e103.bin"
        ),
    )
    parser.add_argument(
        "script",
        nargs="?",
        type=Path,
        help="Python 源文件 (.py)，固件模式可省略",
    )
    parser.add_argument("--port", "-p", help="串口/COM 口，如 COM3（蓝牙转串口填对应 COM）")
    parser.add_argument(
        "--bt",
        action="store_true",
        help=f"蓝牙模式: 上位机串口默认 {BT_HOST_BAUD_DEFAULT}，YMODEM 128B + 透传 pacing",
    )
    parser.add_argument(
        "--baud",
        "-b",
        type=int,
        default=None,
        help=(
            f"上位机 COM 波特率 (USB 默认 {USB_BAUD_DEFAULT}，"
            f"--bt 默认 {BT_HOST_BAUD_DEFAULT}；设备端 UART5 仍为 {BT_DEVICE_BAUD})"
        ),
    )
    parser.add_argument(
        "--firmware",
        "-F",
        type=Path,
        metavar="APP.bin",
        help="固件 .bin：触发 Boot 升级并 YMODEM 发送",
    )
    parser.add_argument(
        "--no-cmd",
        action="store_true",
        help="Pika 模式：不发送 ymodem 命令（设备已在 YMODEM）",
    )
    parser.add_argument("--compile-only", action="store_true", help="仅编译，不发送")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        metavar="OUT.o",
        help="输出 .py.o 路径",
    )
    args = parser.parse_args()

    baud = args.baud
    if baud is None:
        baud = BT_HOST_BAUD_DEFAULT if args.bt else USB_BAUD_DEFAULT

    if args.firmware is not None:
        if not args.firmware.is_file():
            print(f"固件不存在: {args.firmware}", file=sys.stderr)
            return 1
        fw_port = args.port or load_deploy_port(bt=args.bt)
        if not fw_port:
            hint = "deploy_bt_port.txt" if args.bt else "deploy_port.txt"
            print(
                f"固件升级须 --port 或在 tools/{hint} 中配置 COM",
                file=sys.stderr,
            )
            return 1
        rc = run_lbs_fw_update(fw_port, baud, args.firmware.resolve(), bt=args.bt)
        if rc != 0:
            print(f"固件升级失败 (exit={rc})", file=sys.stderr)
            return rc
        print("固件部署完成。")
        return 0

    if args.script is None:
        parser.error("请指定 script.py，或使用 --firmware APP.bin")
        return 2

    print("编译 Python -> .py.o ...")
    py_o = compile_py_to_o(args.script, args.output)
    if args.compile_only:
        return 0

    port = args.port or load_deploy_port(bt=args.bt)
    if not port:
        hint = "deploy_bt_port.txt" if args.bt else "deploy_port.txt"
        print(
            f"未指定 --port，且未找到 tools/{hint}，跳过下载。\n"
            f"一步编译+下发请: python tools/pika_deploy.py {args.script} -p COMx"
            + (" --bt" if args.bt else ""),
            file=sys.stderr,
        )
        return 1

    link = "蓝牙" if args.bt else "USB"
    print(f"下发 ({link}) {port} @ {baud} ...")
    ser = open_bt_serial(port, baud) if args.bt else open_serial(port, baud)

    try:
        crc_timeout = 180.0 if args.bt else 120.0
        if args.no_cmd:
            print("等待设备 'C' (--no-cmd)...", flush=True)
            wait_crc_request(ser, timeout=crc_timeout, bt=args.bt)
        else:
            trigger_device_ymodem(ser, bt=args.bt)
        print("开始传输文件...", flush=True)
        ymodem_send(
            ser,
            py_o,
            block_size=BT_YMODEM_BLOCK if args.bt else USB_YMODEM_BLOCK,
            paced=args.bt,
        )
        brief_drain_after_deploy(ser, bt=args.bt)
    finally:
        safe_close_serial(ser, bt=args.bt)
    print("部署完成。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
