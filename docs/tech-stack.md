# 技术栈

## 平台

| 项 | 值 |
|----|----|
| MCU | APM32E103RE（Cortex-M3，120MHz，512KB Flash / 128KB RAM） |
| 工具链 | Keil µVision 5 (MDK-ARM)，目标名 `SparkAi` |
| 驱动 | STM32F1 HAL + Alientek 系统层（`Drivers/SYSTEM/`） |
| 脚本运行时 | PikaPython（主机端 `python/pikascript-lib/`，Ymodem 下载 .py.o/.py.a 字节码） |
| 部署语言 | Python 3 + pyserial（`tools/` 下脚本） |

## 运行模型

- **无 RTOS**，TIM6 ISR 驱动 `event_manager` 事件调度（EVENT_MANAGER 回调），`main()` 主循环做慢速轮询与挂起消费
- **四层架构**（重构后）：`src/driver`（bsp/sys/chip 外设驱动）→ `src/middleware`（device_pool/bat_manager/drv_comm/event_manager 等）→ `src/protocol`（at/cmd/frame/monitor/ymodem）→ `src/business`（app/devices/pika）
- **依赖规则**：`business → middleware → driver`；`protocol` 不依赖业务层，经抽象接口（回调注册 / `device_pool` / `DrvComm`）与上下层解耦
- **双通道通信**：USB CDC 虚拟串口 + 蓝牙 UART5，独立 ring buffer；Ymodem（USB 1KB STX / 蓝牙 128B SOH）
- **监控上报**：TIM6 ISR → `monitor_event` 事件回调 `monitor_call_back()` 节流发送（USB 1ms / BT 25ms），JSON 序列化，BT 门控在 BLUE_STA(PA8) 连接状态

## 关键协议

- 应用层帧：`0x5A | sID | oID | len | index | data[256] | crc | 0xA5`
- 脚本启停：协议命令 0xB6 / 0xB9 或按键
- 固件升级 / 脚本推送：Ymodem (CRC-16)

## 事件化任务清单（重构后）

| 事件 | 回调 | 所在文件 | 周期 | 职责 |
|---|---|---|---|---|
| `battery_check` | `battery_check_callback` | `src/middleware/bat_manager.c` | 600ms | 电量判定 + 电池 LED |
| `key_middle_event` | `key_middle_callback` | `src/driver/bsp/key.c` | 1ms | 按键状态机（长短按/按住进度/释放），回调上抛给业务层 |
| `monitor_event` | `monitor_call_back` | `src/protocol/monitor.c` | 1ms | 监控 JSON 上报（节流 + BT 门控） |
| `cmd_poll` | `Cmd_PollCallback` | `src/protocol/cmd.c` | 1ms | USB/BT 文本行命令轮询分发 |

## 挂起标志模式

ISR/事件回调中禁止执行耗时/阻塞操作（写 Flash、忙等、关电机动画），改为置挂起标志，由主循环/hook 消费：

- `g_shutdown_pending`（`src/business/app/main.c`）：长按关机 → 主循环 `app_shutdown_sequence()`（写脚本 Flash + 关机动画）
- `g_motor_stop_pending`（`src/business/app/main.c`）：短按停止 → 主循环消费，`cloase_all_motor` 含 60ms 忙等不在 ISR 执行

## 关键路径速查

- **蓝牙两种下载**：PikaPython 脚本（`ymodem` 命令 → `app_run_ymodem` → DrvMem → PikaVM）；APP 固件升级（`ymodem update fmware` → `AppBoot_RequestFirmwareUpdate` → Bootloader）
- **蓝牙 Ymodem 特化**：只收 SOH 128B、空闲超时 15s、周期重发 'C' 间隔 400ms、字节间超时 800ms
- **监控上报**：TIM6 事件 → `monitor_call_back` 节流发送（USB 1ms / BT 25ms），BT 发送门控在 BLUE_STA(PA8) 连接状态；Ymodem 期间双重暂停（`set_event_disable(monitor_event)` + `AppMonitor_SetUploadPaused(1)`）
- **BT 监控单帧上限**：248B（`DRV_BT_MONITOR_TX_MAX`），超出 246B 被 `DrvComm_BtSendMonitor` 静默截断（见 pitfalls.md）
- **蓝牙 AT**：统一经 `src/protocol/at.c` `At_Exchange`（重构前 `blue_at_cmd` + `app_send_bt_at_cmd` 两套已合并）
- **帧协议**：`src/protocol/frame.c` `dataAgreeAnalys` + `frame_parser_*`
- **设备池**：`src/middleware/device_pool.c` `DevicePool_Register/Create/Destroy` 注册表（重构前 `deviceIdentify.c` switch 直调）
- **ADC 采样**：`src/driver/bsp/adc_sample.c` 公共 `AdcSample_*`（重构前 adc.c 与 drv_ir_reflect.c 重复实现）
- **关键文件**：`src/business/app/app_cmd.c`（命令行分发 + Ymodem 入口）、`src/business/app/main.c`（事件注册 + 挂起消费）、`src/protocol/cmd.c`（`cmd_poll` 分发）、`src/protocol/monitor.c`（JSON 构造 + 双通道节流）

## 目录职责

| 目录 | 职责 |
|------|------|
| `src/business/` | 业务层：`app/`（main.c 事件注册/挂起消费、app_cmd、boot_param）、`devices/`（beep/blue/color/motor/touch/ultrasion + matrix）、`pika/`（PikaPython 运行时 + 脚本 Flash 持久化 + ExternLib 绑定） |
| `src/driver/` | 驱动层：`bsp/`（adc_sample/key/led/ir_reflect/spi/iic/stmflash/wdg/btim 等）、`sys/`（delay/sys/usart）、`chip/`（tm1640/w25q80） |
| `src/middleware/` | 中间层：device_pool（注册表）、bat_manager、drv_comm、event_manager、file_manager、USB、RTC、drv_mem/malloc |
| `src/protocol/` | 协议层：at、cmd（`cmd_poll`）、frame、monitor、ymodem、json-maker |
| `python/` | PikaPython 主机端：main.py、*.pyi 绑定、pikascript-core/lib/api |
| `tools/` | 部署/升级工具脚本（烧录、脚本部署、copy_bin） |
| `tests/` | 独立 host 单元测试（脚本 Flash 持久化，gcc 编译运行） |
