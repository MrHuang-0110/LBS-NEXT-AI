# LBS-NEXT-AI

基于 STM32F1 HAL 的嵌入式机器人/教学设备固件，集成 **PikaPython** 脚本运行时。

[![MCU](https://img.shields.io/badge/MCU-APM32E103RE-blue)](https://www.geehy.com)
[![Core](https://img.shields.io/badge/Core-Cortex--M3-green)]()
[![Framework](https://img.shields.io/badge/HAL-STM32F1-orange)]()
[![Script](https://img.shields.io/badge/Script-PikaPython-purple)](https://github.com/pikastech/pikapython)

---

## 目录

- [硬件平台](#硬件平台)
- [内存布局](#内存布局)
- [快速开始](#快速开始)
  - [编译](#编译)
  - [烧录固件](#烧录固件)
  - [部署脚本](#部署脚本)
- [架构](#架构)
  - [调度模型](#调度模型)
  - [通信架构](#通信架构)
  - [监控上报](#监控上报)
  - [PikaPython 运行时](#pikapython-运行时)
  - [脚本持久化](#脚本持久化)
- [代码结构](#代码结构)
- [协议参考](#协议参考)
- [开发约定](#开发约定)
- [已知问题](#已知问题)
- [版本历史](#版本历史)

---

## 硬件平台

| 项目 | 规格 |
|------|------|
| **MCU** | APM32E103RE (Cortex-M3, 120MHz) |
| **Flash** | 512 KB @ `0x08000000` |
| **RAM** | 128 KB @ `0x20000000` |
| **外设** | 电机驱动 · 颜色传感器 · LED 矩阵(TM1640) · 超声波 · 触摸 · 蜂鸣 · BLE 蓝牙 |
| **调试** | SWD (JTAG 已禁用，保留 SWD 用于 Keil 调试) |

## 内存布局

```
0x08000000 ─────────────────────────────  APP 入口 (向量表, VTOR=0x08000000)
0x08000000  ├── ER_IROM1 (352KB) ───────  APP 代码区
0x08060000  ├── 脚本持久化区 (32KB) ─────  单槽, CRC32 校验
0x08068000  ├── ER_IROM2 (64KB) ────────  APP 代码区 (溢出)
0x08078000  ├── 用户配置页 (2KB) ────────  BLE 配对 + 颜色校准
0x08080000 ─────────────────────────────
0x20000000 ─────────────────────────────  RAM (128KB)
```

> 无 Bootloader：APP 链接于 `0x08000000`，Keil 直接烧录。`SystemInit()`（startup 阶段、main 之前）置 `SCB->VTOR = 0x08000000`，中断向量表位于 Flash 起始。

---

## 快速开始

### 环境要求

- **Keil µVision 5** (MDK-ARM)
- **Python 3.x** + `pyserial`
- USB 串口驱动 (CDC 虚拟串口)

```bash
pip install pyserial
```

### 编译

1. 打开 `Projects/MDK-ARM/atk_f103.uvprojx`
2. 选择目标 **SparkAi**
3. 按 **F7** 编译
4. 输出文件: `Output/atk_f103.bin`

编译完成后自动运行 `tools/copy_bin_to.py` 拷贝 `.bin` 到配置的目标目录（编辑 `tools/copy_bin_dest.txt` 修改）。

### 烧录固件

当前无 Bootloader，**仅支持 Keil 直接下载**（Flash 起始 `0x08000000`）。`tools/lbs_fw_update.py` 的 Ymodem 升级依赖 Bootloader 接收，当前不可用；后续若引入 Bootloader 再启用。

> Keil After Build 会自动弹出烧录提示。USB 串口号在 `tools/deploy_port.txt` 中配置。

### 部署脚本

将 PikaPython 脚本编译为字节码并下载到设备：

```bash
# USB 部署
python tools/pika_deploy.py my_app.py -p COM30

# 蓝牙部署
python tools/pika_deploy.py my_app.py -p COM7 --bt

# 仅编译不下载
python tools/pika_deploy.py my_app.py --compile-only

# 等效命令
python tools/lbs_fw_update.py deploy main.py -p COM30
```

---

## 架构

### 调度模型

**协作式 / 事件驱动**，无 RTOS。

TIM6 每 **1ms** 触发 ISR → `event_schedlucer()` 分发注册的事件回调。`main()` 主循环额外轮询监控、命令、电池。

| 事件 | 周期 | 功能 |
|------|------|------|
| `led_flow_event` | 1ms | 流水灯 |
| `iwdg_feedevent` | 10ms | 看门狗喂狗 |
| `usb_connect` | 10ms | USB 连接状态 |
| `scan_adc` | 100ms | ADC 采样 |
| `scan_ir_reflect` | 50ms | 红外反射采样 |
| `usb_receive` | 空闲 5ms | USB 协议帧接收 |
| `beep` | 1ms | 蜂鸣更新 |
| `key_middle_event` | 1ms | 按键扫描 |
| `monitor_event` | 1ms | 监控上报触发 |

> 新增周期性任务应注册为 `EVENT_MANAGER` 条目，而非在主循环中忙等。

### 通信架构

**双通道 + 统一应用层协议**

```
┌─────────────────────────────────────┐
│          应用层协议帧                 │
│  0x5A│sID│oID│len│idx│data│crc│0xA5 │
│        busDataparsing() 分发         │
├─────────────────────────────────────┤
│   USB CDC (1KB STX)  │  BT UART5 (128B SOH) │
│     DRV_COMM_USB     │   DRV_COMM_UART5     │
└─────────────────────────────────────┘
```

**命令字:**

| 命令 | 功能 |
|------|------|
| `0xB6` / `0xB9` | 启动/停止脚本 (toggle) |
| `0xBA` | 启用监控上报 |
| `0xBE` | 禁用监控上报 |

**文本命令** (行分隔，`\r\n` 结尾):

| 命令 | 功能 |
|------|------|
| `ymodem` | 启动 Ymodem 接收 (PikaPython 字节码) |
| `ymodem update fmware` | 启动固件升级 |
| `AT+...` | 蓝牙 AT 命令透传 |

### 监控上报

**JSON Telemetry**，通过 USB 和蓝牙双通道上报。

| 通道 | 周期 | 条件 |
|------|------|------|
| USB | 1ms | `s_monitor_pending` 置位 |
| BT | 25ms | `BLUE_STA`(PA8) 连接状态 |

**上报内容:**

```json
{
  "deviceList": [{"port":0, "ultrasion":{"cm":"123"}}, ...],
  "adc": {"bat":"85%", "ir":"512"},
  "version": 100,
  "heap": "12345",
  "btName": "LBS-01",
  "btAdvData": "...",
  "State": "run"
}
```

- Ymodem 传输期间通过 `set_event_disable("monitor_event")` + `AppMonitor_SetUploadPaused(1)` 双重暂停
- BT 单帧上限 248B (`DRV_BT_MONITOR_TX_MAX`)，超出部分静默截断

### PikaPython 运行时

```
脚本加载流程:
  Ymodem → .py.o/.py.a 字节码 → DrvMem (RAM) → AppPika_LoadBytecode()
  
脚本执行:
  按键/0xB6命令 → start_py=true → main()主循环 → AppPika_Start()
  → pikaVM_runByteCodeInconstant() → VM 执行指令循环
  → 每50条指令 → pika_hook_instruct() → 轮询监控/命令/电池
  
脚本停止:
  按键/0xB6命令 → AppPika_Stop() → s_stop_req=1 → longjmp 退出 VM
```

**Hook 机制 (脚本运行期间保持监控不中断):**

- VM 每 50 条指令触发 `pika_hook_instruct()`
- 每 2 次 hook 调用 `Monitor_Poll` + `AppCmd_PollUsb/Bt` + `check_battery`
- `_os.sleep_s()` 期间每 1ms 调用 `pika_hook_instruct()`，保持监控上报

**安全设计:** `AppPika_Start()` 统一由 `main()` 主循环通过 `start_py` 标志调度，**确保 VM 始终在线程模式运行**，避免在 ISR 上下文中启动 VM 导致死机。

### 脚本持久化

```
Flash 脚本区: 0x08060000 - 0x08067FFF (32KB, 单槽)

数据格式:
  magic(SPyo) | version | length | reserved | script_data | padding(0x1A) | CRC32

行为:
  - Ymodem 下载 → 仅写 RAM (DrvMem)
  - 长按关机 → AppPikaScriptFlash_SaveFromRam() 保存到 Flash
  - 开机 → AppPikaScriptFlash_LoadToRam() 装载到 RAM (不自动运行)
```

---

## 代码结构

```
LBS-NEXT-AI/
├── Users/                          # APP 入口和应用层
│   ├── main.c                      # main(), systemInit(), 主循环, 关机序列
│   ├── app_cmd.c                   # 命令行处理, 协议帧分发, Ymodem入口
│   ├── app_pika_runtime.c          # PikaPython 运行时, Hook, 启动/停止
│   ├── app_pika_script_flash.c     # 脚本 Flash 持久化 (CRC32, 读写)
│   ├── app_boot_param.c            # 启动参数页 (magic + flags)
│   ├── pika_config.h / .c          # PikaPython 平台配置
│   ├── stm32f1xx_it.c              # HAL 中断处理
│   └── stm32f1xx_hal_conf.h        # HAL 配置
│
├── Drivers/
│   ├── BSP/                        # 外设驱动
│   │   ├── drv_flash_storage.c     # Flash 配置存储 (BLE配对, 颜色校准)
│   │   ├── ADC/ IR/ IIC/ KEY/ LED/ SPI/ STMFLASH/ TIMER/ WDG/
│   │   └── drv_ir_reflect.c
│   └── SYSTEM/                     # 正点原子系统层 (sys, delay, usart)
│
├── application/                    # 高层功能模块
│   ├── beep/                       # 蜂鸣控制
│   ├── blue/                       # 蓝牙 BLE (AT命令, 广播数据)
│   ├── color/                      # 颜色传感器
│   ├── matrix/                     # TM1640 LED 矩阵 + UI
│   ├── motor/                      # 电机驱动
│   ├── touch/                      # 触摸传感器
│   └── ultrasion/                  # 超声波传感器
│
├── Middlewares/                    # 中间件
│   ├── protocol/                   # 应用层协议帧解析
│   ├── monitor/                    # JSON 监控构造 + 双通道上报
│   ├── ymodem/                     # Ymodem + drv_comm(ring buffer) + drv_mem
│   ├── event_manager/              # 事件调度器
│   ├── bat_manager/                # 电池管理 (ADC采样 + 去抖)
│   ├── usb/                        # USB CDC 虚拟串口
│   ├── fatfs/                      # 文件系统
│   ├── malloc/                     # 内存管理
│   └── stubs/                      # 硬件缺失桩实现
│
├── python/                         # PikaPython 主机端
│   ├── main.py                     # 脚本入口
│   ├── pikascript-core/            # 核心 VM (不可修改)
│   ├── pikascript-lib/             # 标准库 + 项目定制模块
│   └── pikascript-api/             # 生成的 API 绑定
│
├── tools/                          # Python 部署工具
│   ├── lbs_fw_update.py            # 固件升级 (USB/BT Ymodem)
│   ├── pika_deploy.py              # 脚本编译 + 部署
│   ├── mdk_deploy_prompt.py        # Keil After Build 烧录提示
│   └── copy_bin_to.py              # 编译后自动拷贝 .bin
│
├── Projects/MDK-ARM/               # Keil 工程文件
│   ├── atk_f103.uvprojx            # 主工程
│   └── atk_f103_script.sct         # 自定义散列文件 (脚本区预留)
│
├── tests/                          # 单元测试
│   └── test_script_flash.c         # 脚本 Flash 持久化测试 (13项)
│
├── Output/                         # 编译产物
├── readme.txt                      # 项目说明 (中文 GBK)
└── README.md                       # 本文件
```

---

## 协议参考

### 应用层协议帧

```
┌──────┬─────┬─────┬─────┬───────┬──────────┬─────┬──────┐
│ 0x5A │ sID │ oID │ len │ index │ data[256]│ crc │ 0xA5 │
└──────┴─────┴─────┴─────┴───────┴──────────┴─────┴──────┘
  头     源ID  目标ID 长度   命令      数据     校验   尾
```

### Ymodem 传输

| 通道 | 块大小 | 超时 | 备注 |
|------|--------|------|------|
| USB | 1KB (STX) | 标准 | 固件升级 + 脚本下载 |
| BT | 128B (SOH) | 空闲 15s, 字节间 800ms | 'C' 重发间隔 400ms |

---

## 开发约定

| 规则 | 说明 |
|------|------|
| **类型** | 使用 `uint8_t` / `uint16_t` / `uint32_t` 固定宽度类型 |
| **字面量** | `0U` / `1U` 后缀，对齐 HAL 风格 |
| **返回值** | 忽略时用 `(void)` 显式转换 |
| **注释** | 中文注释；`.c/.h` 用 UTF-8，`.txt` 用 GBK |
| **调试** | 保持 SWD 开启，勿重新启用 JTAG |
| **PikaPython 核心** | `python/pikascript-core/` 不可修改 |
| **样式** | 匹配周围代码的命名、缩进、注释密度 |

---

## 已知问题

### 流水灯少一个灯 (搁置)

脚本下载失败后流水灯 (PC0-PC3) 偶尔少点亮一个，重启后依旧但有时自愈。排查排除了内存越界和调度竞态，高度怀疑 Bootloader 或硬件接触问题。

### Ymodem 提前返回导致监控永久停止 (未修复)

`app_cmd.c:463` 的 `ymodem` 命令处理中，先禁用 `monitor_event` 和暂停上传，若 `DrvComm_Bind` 失败则提前返回，事件和暂停标志都不会恢复。一次失败的 Ymodem 传输会导致监控永久停止，直到重启。

---

## 版本历史

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-07-16 | v1.2 | 修复脚本 sleep 时监控停止 + 0xB6 启动死机 |
| 2026-07-15 | v1.1 | PikaPython 脚本 Flash 持久化 + 编译后自动拷贝 .bin |
| - | v1.0 | 初始版本 (正点原子 MiniSTM32 V4 HAL 模板) |

---

## 参考资料

- [PikaPython](https://github.com/pikastech/pikapython) — 嵌入式 Python 运行时
- [正点原子](https://www.alientek.com) — 开发板厂商
- [STM32Cube MCU Packages](https://www.st.com/en/embedded-software/stm32cube-mcu-mpu-packages.html) — STM32 HAL 库