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

## 6. 初始化阶段 delay_ms 即跑飞（VTOR 向量表错位）✅ 已修复（2026-08-05）

- **现象**：`systemInit` 早期（`__enable_irq()` 之前）调用 `delay_ms` 必跑飞；Keil 停在 `0xA0000000` / `0x60080002`（无效地址）无法单步，HardFault lockup。不加 delay 时偶发不跑飞（纯属时序侥幸）
- **根因**：`SystemInit()`（startup 调用、main 之前）执行 `SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET`。工程已改为**无 Bootloader、APP 链接 `0x08000000`（Keil 直烧）**，但 `system_stm32f1xx.c` 的 `VECT_TAB_OFFSET` 仍是旧 Bootloader 布局的 `0x8000` → VTOR=`0x08008000`，向量表错位：SysTick 向量从 `0x0800803C` 读到 `0x60080002`（恰好是代码区指令编码）→ 中断触发即跳飞。`delay_ms` 忙等只是给了 SysTick 中断 1ms 触发窗口，是**催化剂而非根因**
- **方案**：
  - `Drivers/CMSIS/.../system_stm32f1xx.c`：`VECT_TAB_OFFSET` `0x8000` → `0x0000`（VTOR=`0x08000000`，与向量表实际位置一致）
  - 同步配套：uvprojx IROM=`0x08000000`、`.eide/eide.yml` IROM startAddr=`0x8000000`、main.c `sys_nvic_set_vector_table(FLASH_BASE, 0x8000)` 保持注释
  - 纵深防御：`btim_timx_int_start()` 与 `set_event_enable("monitor_event")` 移到 `systemInit` 末尾（event 注册默认全 enable，TIM6 早启动会在 USB 枚举/blue_init/按键回调注册完成前触发 monitor/key/usb_receive 回调）
- **铁律**：无 Bootloader 布局下 VTOR 必须指向 `0x08000000`；初始化完成前不启动 TIM6 事件调度器；`delay_ms` 忙等本身安全，跑飞必先查中断向量表/VTOR，再查回调时序

## 7. 关机动画被按键事件干扰 + Event_t.name 越界 ✅ 已修复（2026-08-06）

- **现象**：长按关机触发后，关机动画（GPIO 流水灯闪烁/逐个熄灭）被干扰——全部 LED 很暗地闪烁、拖 2 秒才关机，与重构前表现不一致
- **根因**（两层）：
  1. 重构后按键状态机移入 TIM6 ISR 的 `key_middle_event`（1ms 一次），关机序列阻塞播动画期间 ISR 照跑；长按触发后手指未松 → `press_start_time` 清零重计时 → `hold_progress` 回调持续上报 `lit=0→3→6→9` 循环，反复改写流水灯 GPIO 与关机动画抢控制权；松手还触发 release 回调重新开流水灯。重构前按键扫描在主循环、关机后 `while(1)` 死循环停止扫描，故无此问题
  2. 隐藏 bug：`Event_t.name[16]` 装不下 16 字符事件名 `"key_middle_event"`（+`\0`=17B），`strcpy` 越界写 → `find_event` 的 `strcmp` 永远失败 → `set_event_disable("key_middle_event")` 从未生效
- **方案**：`Event_t.name[16]`→`name[20]`、`enabled` 加 `volatile`、`strcpy`→`strncpy`；`app_shutdown_sequence()` 开头 `set_event_disable("key_middle_event")`
- **铁律**：ISR 里每 1ms 跑的回调若会改共享硬件状态（LED/蜂鸣），主流程阻塞动画期间必须停用对应事件；事件名数组长度须按最长事件名+`\0` 设计，`strcpy` 一律改有界拷贝


## 8. USB/蓝牙脚本启停指令（0xB6/0xB9）不生效 ✅ 已修复（2026-08-06）

- **现象**：上位机通过 USB 或蓝牙发送 0xB6（启动脚本）/0xB9（停止脚本）指令帧无任何反应；带数据的帧（如 0xC1 蓝牙遥控）正常
- **根因**（三层）：
  1. `dataAgreeAnalys()`（`src/protocol/frame.c`）有 `length < 8` 硬检查，但 0xB6/0xB9/0xBE/0xBA 均为**无数据帧**，总长只有 7 字节（`0x5A|sID|oID|len=0|idx|data(0)|crc|0xA5`）→ USB、蓝牙两条通道的指令帧全部被拒（蓝牙先被拒于此处，未走到设备分发）
  2. `usb_event_receive_callback`（`src/middleware/usbd_cdc_interface.c`）用**整批 buffer + 整批长度**做二次解析，而 `frame_parser_process_byte` 已逐字节找出完整帧（buffer+index）；一次 USB 接收含前导字节/多帧合并时，整批长度与单帧 CRC 不匹配 → 指令帧被丢弃
  3. 蓝牙 UART5 路径（`src/driver/sys/usart.c`）解析成功只走 `set_sensor_parameter`→`refsh_blue`，而 `refsh_blue` 只处理 index 0xC1，0xB6/0xB9 到不了 `Cmd_ProcessFrame` 统一分发
- **方案**：
  - `frame.h` `MIN_FRAME_SIZE` 8→7；`dataAgreeAnalys` 长度检查前移（先查长度再解引用 `data[length-1]`，防越界），并校验 `data[3] <= length-7`（帧总长 = 7 + data[3]）防截断帧把 crc/0xA5 当数据
  - `usb_event_receive_callback` 改用 `message->usb_parser.buffer/index` 解析，解析后复位状态机以支持一包多帧；去掉重复 `frame_parser_init`（`reset_usb_parser` 内含）
  - UART5 解析成功分支补 `Cmd_ProcessFrame(&frame)`（0xC1 未注册 action 被忽略，无副作用）
- **铁律**：帧最小长度按 `0x5A|sID|oID|len|idx|data[len]|crc|0xA5` 计算 = `len+7`，len=0 时为 7 字节，长度检查不得写死 8；解析以 parser 找到的帧为准，勿用整批 buffer 长度重解

## 9. 运行脚本前未关闭流水灯（Python 环境不干净） ✅ 已修复（2026-08-06）

- **现象**：脚本运行期间 GPIO 流水灯（PC0-PC3 等）仍在跑，与 Python 侧 `set_point_matrix` 等灯光控制抢占 LED
- **根因**：`AppPika_Start()` 启动 VM 前不关流水灯，仅当脚本自身调用 `os.set_point_matrix`（内部置 `s_flow_enable=0`）才停；按键 release 回调还会无条件 `DrvLed_SetFlowEnable(1U)` 重开
- **方案**：`AppPika_Start()` 置 RUNNING 后立即 `DrvLed_SetFlowEnable(0U)`；`app_key_release` 在脚本 RUNNING 时不再重开流水灯（停止后由 `AppPika_Start` 尾部统一恢复）
- **铁律**：脚本启动路径（命令/按键）统一在 VM 运行前关掉非脚本侧动画，停止后恢复，保证 Python 有干净外设环境
