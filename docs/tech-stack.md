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

- **无 RTOS**，协程/事件驱动：TIM6 ISR 驱动 `event_schedlucer()` 调度 EVENT_MANAGER 回调，`main()` 主循环做慢速轮询
- **双通道通信**：USB CDC 虚拟串口 + 蓝牙 UART5，独立 ring buffer；Ymodem（USB 1KB STX / 蓝牙 128B SOH）
- **监控上报**：TIM6 ISR → pending 标志 → `Monitor_Poll()` 节流发送（USB 1ms / BT 25ms），JSON 序列化，BT 门控在 BLUE_STA(PA8) 连接状态

## 关键协议

- 应用层帧：`0x5A | sID | oID | len | index | data[256] | crc | 0xA5`
- 脚本启停：协议命令 0xB6 / 0xB9 或按键
- 固件升级 / 脚本推送：Ymodem (CRC-16)

## 关键路径速查

- **蓝牙两种下载**：PikaPython 脚本（`ymodem` 命令 → `app_run_ymodem` → DrvMem → PikaVM）；APP 固件升级（`ymodem update fmware` → `AppBoot_RequestFirmwareUpdate` → Bootloader）
- **蓝牙 Ymodem 特化**：只收 SOH 128B、空闲超时 15s、周期重发 'C' 间隔 400ms、字节间超时 800ms
- **监控上报**：TIM6 ISR → `monitor_call_back` 置 pending → `Monitor_Poll` 节流发送（USB 1ms / BT 25ms），BT 发送门控在 BLUE_STA(PA8) 连接状态；Ymodem 期间双重暂停（`set_event_disable(monitor_event)` + `AppMonitor_SetUploadPaused(1)`）
- **BT 监控单帧上限**：248B（`DRV_BT_MONITOR_TX_MAX`），超出 246B 被 `DrvComm_BtSendMonitor` 静默截断（见 pitfalls.md）
- **关键文件**：`Users/app_cmd.c`（命令行分发 + Ymodem 入口）、`Middlewares/ymodem/drv_ymodem.c`（协议状态机）、`Middlewares/ymodem/drv_comm.c`（双通道 ring buffer）、`Middlewares/monitor/monitor.c`（JSON 构造 + 双通道节流）

## 目录职责

| 目录 | 职责 |
|------|------|
| `Users/` | APP 入口(main.c)、命令分发(app_cmd.c)、Pika 运行时(app_pika_runtime.c)、HAL 配置 |
| `Drivers/BSP/` | 外设驱动：ADC、IIC、IR、KEY、LED、SPI、STMFLASH、TIMER、WDG |
| `Drivers/SYSTEM/` | Alientek 系统层（时钟/延时/系统/串口） |
| `application/` | 功能模块：beep、blue(BLE)、color、matrix(TM1640)、motor、touch、ultrasion |
| `Middlewares/` | protocol、monitor、ymodem、event_manager、bat_manager、fatfs、usb、RTC 等；`stubs/` 为缺失硬件桩 |
| `python/` | PikaPython 主机端：main.py、*.pyi 绑定、pikascript-core/lib/api |
| `tools/` | 部署/升级工具脚本（烧录、脚本部署、copy_bin） |
| `tests/` | 独立单元测试（脚本 Flash 持久化等） |
