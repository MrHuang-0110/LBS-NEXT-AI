# 踩坑记录（Pitfalls）

> 格式：现象 → 原因 → 方案。已修复的保留备查，未修复的标注状态。

## 1. 脚本 sleep 期间监控停止上报 ✅ 已修复（2026-07-16）

- **现象**：PikaPython 脚本 `sleep_s` 期间监控停止上报，motor 命令执行期间短暂恢复
- **根因**：`_os_sleep_s`（`python/pikascript-lib/ExternLib/_os.c`）用 `delay_ms(50)` 忙等，VM 指令循环暂停，`pika_hook_instruct` 不触发，`Monitor_Poll` 不被调用
- **方案**：改为 `pika_hook_instruct() + delay_ms(1)` 循环，1ms 粒度轮询 hook
- **附带收益**：sleep 期间也能响应 0xB6 停止命令、接收 USB/BT 协议帧

## 2. 0xB6 命令启动脚本死机 ✅ 已修复（2026-07-16）

- **现象**：USB 协议帧 0xB6 启动脚本时设备死机；按键启动正常，0xB6 停止脚本正常
- **根因**：0xB6 由 `usb_event_receive_callback`（TIM6 ISR 上下文）处理，`busDataparsing` 直接调用 `AppPika_Start()`，PikaPython VM 在 ISR 上下文运行；setjmp/longjmp、深调用栈、delay_ms 忙等均不可靠 → HardFault/死锁
- **方案**：`AppPika_Start()` 统一从 `busDataparsing` / `AppPika_OnKeyToggle` 移到 `main()` 主循环，通过 `start_py` 标志调度，保证 VM 始终在线程模式运行
- **铁律**：**任何情况下不得在 ISR 上下文启动/运行 PikaPython VM**

## 3. 流水灯少一个灯 ⏸ 搁置

- **现象**：脚本下载失败后流水灯（PC0-PC3）少点一个灯，重启依旧，但有时自愈
- **已排查**：蜂鸣 PA1/TIM5、电池 ADC PA0、LED 点阵 PB6/PB7、电机 PWM、UART5 均与 PC0-PC3 无关；失败路径 `app_run_ymodem` ok==0 分支无相关代码；排除内存越界与调度竞态（`s_flow_idx` 仅 ISR 上下文读写）
- **怀疑**：Bootloader（0x08000000 前 32KB，不在本仓库）启动时动了 PC0-PC3 的 MODER/CRL，或硬件接触不良
- **待查**：`iwdg_init` 在 main.c:190 被注释，但 `iwdg_feedevent` 事件仍注册，需确认是否另有地方初始化看门狗

## 4. 蓝牙监控帧静默截断 ⚠ 已知限制

- BT 监控单帧上限 248B（`DRV_BT_MONITOR_TX_MAX`），超出 246B 会被 `DrvComm_BtSendMonitor` 静默截断
- 构造监控 JSON 时注意控制单帧体积

## 5. 编码坑：GBK vs UTF-8

- 旧 `.txt` 文件为 GBK 编码；`.c/.h/.py` 为 UTF-8。跨工具读写文本文件时注意编码，避免乱码/编译异常
