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

## 2026-08-06 — 关机动画被按键事件干扰修复 ✅

- **现象**：长按关机触发后，关机动画（GPIO 流水灯闪烁/逐个熄灭）被干扰——全部 LED 很暗地闪烁、拖 2 秒才关机，与重构前表现不一致
- **根因**（两层）：
  1. 重构后按键状态机移入 TIM6 ISR 的 `key_middle_event`（1ms 一次），关机序列阻塞播动画期间 ISR 照跑；长按触发后手指未松 → `press_start_time` 清零重计时 → `hold_progress` 回调持续上报 `lit=0→3→6→9` 循环，反复改写流水灯 GPIO，与关机动画抢控制权；松手还会触发 release 回调重新打开流水灯。重构前按键扫描在主循环、关机后 `while(1)` 死循环停止扫描，故无此问题
  2. 隐藏 bug：`Event_t.name[16]` 装不下 16 字符事件名 `"key_middle_event"`（+`\0`=17B），`strcpy` 越界写导致 `find_event` 的 `strcmp` 永远失败 → `set_event_disable("key_middle_event")` 从未生效
- **方案**：
  - `event_manager.h`：`Event_t.name[16]` → `name[20]`；`enabled` 加 `volatile`（主循环写/ISR 读）
  - `event_manager.c`：`strcpy` → `strncpy`（防未来事件名再越界）
  - `app_shutdown_sequence()`：开头 `set_event_disable("key_middle_event")`，恢复重构前"关机后按键处理停止"行为
- **验证**：新增 host 测试 `tests/test_shutdown_flow.c` **4/4 PASS**（含 16 字符事件名可禁用回归测试）；原有 13/13 仍 PASS；上板验证见待办

## 待办 / 风险

- [ ] Keil F7 编译验证自定义散列文件（atk_f103_script.sct），烧录测试脚本持久化流程（手动）
- [ ] 关机动画修复上板验证：长按关机时动画应干净播放不被干扰（手动）
- [ ] 搁置 Bug：流水灯少一个灯（见 docs/pitfalls.md）

## 2026-08-06 鈥?鍏虫満铚傞福銆滃緢灏忓０/鍝嶄笉璧锋潵"淇?鈽?
- **鐜拌薄**锛氬叧鏈烘祦绋嬶紙LED 鍔ㄧ敾锛夋甯稿悗锛岀3 绉掑叧鏈烘棆寰嬪紑濮嬫挱鏀炬椂铚傞福鍣ㄥ嚑涔庢棤澹帮紝鍚彉"寰堝皬澹?鍝嶄笉璧锋潵"
- **鏍瑰洜**锛歜eep_play_shutdown_melody_blocking 璧?beep_play_piano_melody锛屾瘡闊︾灏?motor_delay_exit(120) 闃诲涓?motor.c:64 鍐呴儴姣?1ms 鏌?VMSignal_getCtrl()==VM_SIGNAL_CTRL_EXIT 鍛藉腑绔嬪嵆 break锛涘叧鏈哄墠鑴氭湰琚?AppPika_Stop 鍋滄銆佹垨鍏虫満杩囩▼涓?USB/BT 鏀跺埌 0xB9 鍋滄鎸囦护 鈫?signal_ctrl 涓?EXIT 鈫?7 涓闊︾灏?0ms 鍏ㄩ儴缁撴潫锛岃€屼笖 pika_vmSignal_setCtrlClear 鍦ㄦ棆寰嬪墠宸叉竻 EXIT锛屼絾鏃犳硶闃叉鎾斁鏃跺啀娆¤绔?EXIT锛堝叧鏈轰腑 USB/BT 鏂囨湰鍛戒护缁?Cmd_ProcessFrame 鍙兘璁剧疆锛?
- **鏂规**锛歜eep_play_shutdown_melody_blocking 鏀圭敤 beep_play_melody(鈥?84,659,523,392,330,262,196鈥?120)锛圚AL_Delay 绾樿寮忛樆濉★紝涓嶆鏌?VM 淇″彿锛夛紱G5,E5,C5,G4,E4,C4,G3 瀵瑰簲棰戠巼 784,659,523,392,330,262,196Hz
- **楠岃瘉**锛氭柊澶?host 娴嬭瘯 tests/test_shutdown_melody.c 4/4 PASS锛涘師鏈?test_shutdown_flow 4/4銆乼est_script_flash 13/13 鍏ㄩ儴 PASS锛涗笂鏉块獙璇佽寰呭姙
- **根因确认**（贤哥定位）：长按 1.5s 触发 tick 内，第三声进度蜂鸣（beep_play(BEEP_KEY_PRESS)，800Hz×30ms）刚启动，app_shutdown_sequence 开头立即 beep_stop() 把它掐成"哒"一声（且 beep_play 内部首行也会 beep_stop）；关机音效排在动画之后才播，时序错乱
- **方案落地**（2026-08-06 定稿）：main.c app_shutdown_sequence 去掉开头 beep_stop，保存 Flash 后 while(beep_is_playing()) 等第三声自然播完（≤30ms），再 beep_play(BEEP_POWER_OFF) 非阻塞启动关机音效与动画（1500ms）同步播放，动画结束 beep_stop 兜底断电；beep.c power_off_sound 加长为 11 音符≈1300ms；删除死代码 beep_play_shutdown_melody_blocking；测试 tests/test_shutdown_melody.c 4/4 PASS，全量 4+4+13 PASS
