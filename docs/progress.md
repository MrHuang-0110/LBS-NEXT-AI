# 项目进度

> 维护：每次里程碑完成同步更新。本文件是唯一记忆系统的一部分（连同 pitfalls.md / tech-stack.md）。

## 2026-07-22 — 项目初始化重构

- CLAUDE.md 移除，改 AGENTS.md（Reasonix 规范）
- MCP memory 知识谱弃用（.memory/ 与 .mcp.json 删除），记忆统一维护在 docs/ 文档体系
- docs/progress.md（进度）/ pitfalls.md（坑）/ tech-stack.md（技术栈）建立

## 2026-07-22 — LED/电机 API 扩展（提交 `bf6c9af`）

- 流水灯 4 灯 → 9 灯（PC0→PC1→PC2→PC3→PC13→PB7→PB6→PB5→PB2），蓝牙灯 PC15 独立闪烁
- 新增 `_os_set_port_mode(port, mode)`：port=0→电机ID6, port=1→电机ID7；mode=0 电机模式(不操作)、mode=1 串口模式(正转满速100)
- 串口模式状态记录 `s_port_serial_mode`，脚本退出时 `cloase_all_motor` 跳过串口端口；新增 `os_is_port_serial_mode()` 查询
- 关机长按改为 1.5s，每 0.5s 亮一组(3 个 LED)，全亮后立即进入关机动画
- `set_point_matrix(point, state)` 增加 state 参数：point=0~8，state=0 熄灭 / 1 点亮，单独控制不影响其他 LED
- Python 脚本运行时系统不再接管 LED 流水灯（控制权完全交脚本），脚本停止后自动恢复系统流水灯

## 2026-07-16 — 两个 Bug 修复

- 脚本 `sleep_s` 期间监控停止上报 → 根因 `delay_ms(50)` 忙等阻断 VM hook，已修复
- 0xB6 命令启动脚本死机 → 根因 ISR 上下文运行 VM，已修复（改主循环 `start_py` 标志调度）

## 2026-07-15 — PikaPython 脚本 Flash 持久化 ✅

- 设计→实施→审查→测试全流程完成，13 个单元测试全部 PASS
- 脚本区 `0x08060000-0x08067FFF`，32KB 单槽，CRC32 校验，仅长按关机保存、启动自动装载不运行

## 2026-08-05 — 全项目重构完成 ✅（Task 1-21，分支 main-work）

- **四层分层**：代码全部归位 `src/driver|middleware|protocol|business`（bsp/sys/chip → driver；bat_manager/device_pool/drv_comm/event_manager 等 → middleware；at/cmd/frame/monitor/ymodem → protocol；app/devices/pika → business），Keil 工程同步（提交 `c2907a6` `e9a1fa5` `c83e45e` `e3118d6` `f32577d`）
- **实时任务事件化**：TIM6 事件驱动，`battery_check`（`da68fef`）、按键状态机 `key_middle_event`（`e36c341`）、`monitor_event`（`5aab3d0`）、`cmd_poll`（`437ae0e`），hook 精简为非实时兜底（`e1a8952`）
- **接口抽象**：`device_pool` 注册表化（`5e1108a`）、monitor 经 device_pool 读传感器（`4959d34`）、blue 走 DrvComm（`e53eca3`）、USB 回调分发（`eaaef5e`）、fatfs/exfuns 回调访问 UI（`f2dcc92`）
- **冗余治理**：蓝牙 AT 两套统一到 `protocol/at`（`75a4099`）、ADC 采样公共化（`3f30d4c`）、btim 空实现清理（`a0e8e01`）、遗留 .bak/死代码/空目录删除（`e64522f`）、中文注释编码修复（`dc66eb3`）
- **验证**：全量编译 **0 Error(s), 0 Warning(s)**（Keil F7，`Output/atk_f103.bin` 生成）；host 测试 `tests/test_script_flash.c` **13/13 PASS**（gcc 编译零警告）；功能对照表见 `docs/refactor-function-mapping.md`

## 待办 / 风险

- [ ] Keil F7 编译验证自定义散列文件（atk_f103_script.sct），烧录测试脚本持久化流程（手动）
- [ ] 搁置 Bug：流水灯少一个灯（见 docs/pitfalls.md）
