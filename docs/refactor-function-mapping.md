# 重构功能对照表

> 全项目四层重构（Task 1-21）功能映射：重构前位置 → 重构后位置 → 验证方法。
> 重构前位置以 `1149d4c`（初始提交）为准；重构后位置以当前 main-work 分支 HEAD（`dc66eb3`）为准。
> 验证方法中"上板/上位机/Ymodem 实测"均需贤哥上板逐项验证。

| 功能 | 重构前位置 | 重构后位置 | 验证方法 |
|---|---|---|---|
| 开机流程 | `systemInit`（`Users/main.c:164-210`，1149d4c） | `src/business/app/main.c:151` `systemInit` | 上板 |
| 按键短按切脚本 | `Main_Loop_Process` 手工时序（`Users/main.c:85-162`，短按判定 141-153，1149d4c） | `src/driver/bsp/key.c` 状态机 + 回调 `Key_RegisterShortPressCb` → `app_key_short_press`（main.c:100）→ `AppPika_Stop`/`start_py` | 上板 |
| 按键长按关机 | `Main_Loop_Process`（`Users/main.c:134-138`，1149d4c） | `src/driver/bsp/key.c` 状态机 + `g_shutdown_pending`（main.c:29/126）→ hook/主循环 `app_shutdown_sequence`（main.c:80/217-220） | 上板 |
| 电量判定+电池 LED | `check_battery_with_debounce`（`Middlewares/bat_manager/bat_manager.c:101`，1149d4c） | `src/middleware/bat_manager.c:137` `battery_check_callback`（`battery_check` 事件，TIM6 600ms） | 上板 |
| 监控 JSON 上报 | `Monitor_Poll`（`Middlewares/monitor/monitor.c:131`，1149d4c） | `src/protocol/monitor.c:152` `monitor_call_back`（`monitor_event` 事件） | 上位机观察 |
| USB/BT 文本行命令 | `AppCmd_PollUsb`/`AppCmd_PollBt`（`Users/app_cmd.c:544/557`，1149d4c） | `src/protocol/cmd.c:125` `Cmd_PollCallback`（`cmd_poll` 事件）→ `Cmd_PollUsb/Cmd_PollBt` | 上位机 |
| Ymodem 脚本推送 | `app_run_ymodem`（`Users/app_cmd.c:104`，1149d4c） | `src/business/app/app_cmd.c:40` `app_run_ymodem` | Ymodem 实测 |
| 蓝牙 AT 命令 | `blue_at_cmd`（`application/blue/blue.c:48`）+ `app_send_bt_at_cmd`（`Users/app_cmd.c:383`）两套 | `src/protocol/at.c:268` `At_Exchange`（统一） | USB 串口 AT 实测 |
| 帧协议解析 | `dataAgreeAnalys`（`Middlewares/protocol/protocol.c:13`，1149d4c） | `src/protocol/frame.c:13` `dataAgreeAnalys` + `frame_parser_*` | 上位机 |
| 设备池 | `Middlewares/deviceIdentify/deviceIdentify.c`（switch 直调 create_*，1149d4c） | `src/middleware/device_pool.c`（注册表 `DevicePool_Register/Create/Destroy`） | 上板 |
| ADC 采样 | `Drivers/BSP/ADC/adc.c` + `Drivers/BSP/IR/drv_ir_reflect.c`（重复实现，1149d4c） | `src/driver/bsp/adc_sample.c`（公共 `AdcSample_*`） | 上板 |

## 行为变化

**唯一有意行为变化**（Task 3，提交 `e36c341`）：脚本运行期间按键可用 —— 短按停止脚本、长按关机。
重构前主循环忙等运行脚本时按键判定失效；重构后按键判定移入 `key_middle_event` 事件（TIM6 事件回调），
脚本阻塞期间仍可短按停止 / 长按关机（关机序列经 `g_shutdown_pending` 挂起到主循环消费，ISR 安全）。

其余功能行为均保持不变（编译门禁 0 Error / 0 Warning 通过，host 测试 13/13 PASS）。
