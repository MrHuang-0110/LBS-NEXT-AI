# LBS-NEXT-AI — 项目工作守则

主人：贤哥。会话开始时主动问好。原则：**先想清楚再动手，最小改动解决问题，有疑问先问。**

## Project

嵌入式机器人/教学设备固件，目标芯片 **APM32E103RE**（Cortex-M3，512KB Flash / 128KB RAM），Keil µVision (MDK-ARM) + STM32F1 HAL 构建，集成 **PikaPython** 脚本运行时（Ymodem 下载字节码、运行时执行）。

- 入口：`Users/main.c`；APP 链接于 `0x08008000`，前 32KB 为 Bootloader（不在本仓库）
- 硬件：电机 · 颜色传感器 · LED 点阵(TM1640) · 超声波 · 触摸 · 蜂鸣 · BLE
- 文档入口：`README.md`（架构/协议/开发约定）、`docs/`（进度/坑/技术栈）

## Commands

| 操作 | 命令 |
|------|------|
| 编译 | Keil 打开 `Projects/MDK-ARM/atk_f103.uvprojx`，目标 **SparkAi**，F7；产物 `Output/atk_f103.bin`（After Build 自动跑 `tools/copy_bin_to.py`） |
| 烧录固件(USB) | `python tools/lbs_fw_update.py -p COM30 -f Output/atk_f103.bin` |
| 烧录固件(蓝牙) | `python tools/lbs_fw_update.py -p COM7 --bt -f Output/atk_f103.bin` |
| 部署脚本 | `python tools/pika_deploy.py my_app.py -p COM30`（`--compile-only` 仅编译；端口见 `tools/deploy_port.txt`） |
| 依赖 | `pip install pyserial` |
| 清理中间文件 | `keilkill.bat` |

> `tools/`、`keilkill.bat`、`postbuild.bat` 为本地工具（不进 git，仅本机可用）；上表命令依赖它们，其他环境需自行提供等价工具。

无 Makefile/测试框架/linter；`tests/` 下有独立单测（脚本 Flash 持久化等），用 Keil/桌面工具或手动验证。

## Architecture

- **调度**：协程/事件驱动，无 RTOS。TIM6 ISR → `event_schedlucer()` 按毫秒阈值调度 EVENT_MANAGER 回调（LED、IWDG、USB、ADC、按键、监控…）；`main()` 主循环轮询 `Monitor_Poll()`、`AppCmd_PollUsb/Bt()`。新增周期工作走 EVENT_MANAGER 条目，勿忙等。
- **通信**：双通道 — USB CDC + 蓝牙 UART5，各自 ring buffer，帧协议 `0x5A|sID|oID|len|index|data|crc|0xA5`，`busDataparsing()`（`Users/app_cmd.c`）分发；Ymodem 用于固件升级 + Pika 字节码推送。
- **PikaPython**：`Users/app_pika_runtime.c`；`pika_hook_instruct()` 每 N 条 VM 指令调用以保持调度器活跃；脚本启停 = 协议 0xB6/0xB9 或按键，**启动统一经 `start_py` 标志在 main() 主循环执行，严禁在 ISR 上下文启动 VM**。
- **Flash 映射**：`0x08008000` APP / `0x08060000` 脚本区 32KB(CRC32) / `0x08078000` 用户配置页(BLE 配对+颜色校准，key `PKCL`/`ECB2`)。
- **目录**：`Users/` APP 入口与分发 · `Drivers/BSP/` 外设驱动 · `Drivers/SYSTEM/` Alientek 系统层 · `application/` 功能模块(motor/blue/matrix/…) · `Middlewares/` ymodem/monitor/event_manager/fatfs/usb 等 · `python/` Pika 主机端 · `tools/` 部署脚本。

## Conventions

- **改代码后的强制流程（任何代码改动，含修 bug，缺一不可）**：
  1. **TDD 测试** — 先写/补测试（test-driven-development），确保改动有测试覆盖且全部通过；
  2. **代码审查** — 用 review 技能审查本次改动（正确性 / 安全性 / 遗漏）；
  3. **code simple 优化** — 代码简化（去冗余、保持最小改动）。
- 代码：`uint8_t/uint32_t` 固定宽度类型；整数常量加 `0U/1U` 后缀；忽略返回值显式 `(void)`；新代码中文注释；`main.c` 中保持 SWD 调试口（`disable_jtag_enable_swd()`），勿重新启用 JTAG。
- 编码：`.c/.h/.py` 用 UTF-8；旧 `.txt` 为 GBK。
- 记忆系统（**唯一**，docs/ 文档体系，已弃用 MCP memory 知识谱）：里程碑完成 → 更新 `docs/progress.md`；踩坑 → 记入 `docs/pitfalls.md`（现象→原因→方案，标注状态）；技术栈/架构变化 → 更新 `docs/tech-stack.md`。只记项目级重要信息，保持精简可读。
- Windows 环境：脚本里 `python3` 一律改用 `python`。
- 匹配场景必须主动调用技能（不等 `/` 命令）；技能未安装不得伪造输出，改用通用能力并提示。

## Notes

（待补充）
