/**
 ***************************************************************************************************
 * LBS-NEXT-AI 固件项目
 * ====================
 * 基于 STM32F1 HAL 的嵌入式机器人/教学设备固件，集成 PikaPython 脚本运行时。
 * 本项目为 APP 镜像，依赖独立 Bootloader 启动。
 *
 ***************************************************************************************************
 * 硬件平台
 * --------
 * MCU:      APM32E103RE (Cortex-M3, 120MHz)
 * Flash:    512 KB @ 0x08000000
 * RAM:      128 KB @ 0x20000000
 * 外设:     电机驱动、颜色传感器、LED 矩阵(TM1640)、超声波、触摸、蜂鸣、蓝牙(BLE)
 * 调试口:   SWD (JTAG 已禁用，保留 SWD 用于 Keil 仿真)
 *
 ***************************************************************************************************
 * 内存布局
 * --------
 * 0x08000000 - 0x08007FFF   Bootloader (32KB, 不在本仓库)
 * 0x08008000 - 0x0805FFFF   APP 代码区 (352KB, ER_IROM1)
 * 0x08060000 - 0x08067FFF   脚本持久化区 (32KB, 单槽)
 * 0x08068000 - 0x08077FFF   APP 代码区 (64KB, ER_IROM2)
 * 0x08078000 - 0x0807FFFF   用户配置页 (2KB, BLE配对+颜色校准)
 * 0x20000000 - 0x2001FFFF   RAM (128KB)
 *
 * 注意: 设备上电从 Bootloader 启动，Bootloader 验证后跳转到 0x08008000。
 *       APP 入口处调用 sys_nvic_set_vector_table(FLASH_BASE, 0x8000) 重定位向量表。
 *
 ***************************************************************************************************
 * 编译与部署
 * ==========
 *
 * 编译环境
 * --------
 * IDE:      Keil µVision 5 (MDK-ARM)
 * 工程文件:  Projects/MDK-ARM/atk_f103.uvprojx
 * 目标名称:  SparkAi
 * 输出目录:  Output/ (atk_f103.axf, atk_f103.bin)
 * 编译选项:  USE_HAL_DRIVER, STM32F103xE, PIKA_CONFIG_ENABLE
 *
 * 编译
 * ----
 * 1. 打开 Projects/MDK-ARM/atk_f103.uvprojx
 * 2. 选择目标 SparkAi
 * 3. 按 F7 编译
 * 4. 编译完成后自动运行 tools/copy_bin_to.py 拷贝 .bin 到目标目录
 *    (目标目录在 tools/copy_bin_dest.txt 中配置)
 *
 * 烧录 (APP .bin -> 设备，通过 Bootloader Ymodem)
 * ------------------------------------------------
 * USB CDC:
 *   python tools/lbs_fw_update.py -p COM30 -f Output/atk_f103.bin
 *
 * 蓝牙 UART:
 *   python tools/lbs_fw_update.py -p COM7 --bt -f Output/atk_f103.bin
 *
 * Keil After Build 会自动弹出烧录提示对话框。
 * USB 串口号在 tools/deploy_port.txt 中配置 (当前 COM30)。
 *
 * 清理编译产物
 * ------------
 *   keilkill.bat
 *
 * 依赖
 * ----
 *   pip install pyserial
 *
 ***************************************************************************************************
 * PikaPython 脚本部署
 * ===================
 *
 * 编译 .py -> 字节码 -> 设备
 * --------------------------
 * USB 部署:
 *   python tools/pika_deploy.py my_app.py -p COM30
 *
 * 蓝牙部署:
 *   python tools/pika_deploy.py my_app.py -p COM7 --bt
 *
 * 仅编译不下载:
 *   python tools/pika_deploy.py my_app.py --compile-only
 *
 * 等效命令:
 *   python tools/lbs_fw_update.py deploy main.py -p COM30
 *
 * 脚本编译使用 PikaCompiler (rust-msc-latest-win10.exe, 位于 python/ 和 tools/)。
 * 入口脚本: python/main.py
 * 模块绑定: python/_*.pyi 存根文件
 *
 * 脚本持久化
 * ----------
 * 脚本下载后存储在 RAM 中。长按关机时自动保存到 Flash 脚本区(0x08060000)。
 * 下次开机时自动装载到 RAM (不自动运行)，可通过按键或 0xB6 命令启动。
 * 数据格式: magic(SPyo) + version + length + reserved + data + padding(0x1A) + CRC32
 *
 ***************************************************************************************************
 * 架构概述
 * ========
 *
 * 调度模型: 协作式/事件驱动 (无 RTOS)
 * ---------------------------------
 * TIM6 每 1ms 触发一次，ISR 中调用 event_schedlucer() 分发注册的事件回调。
 * main() while(1) 循环额外轮询 Monitor_Poll、AppCmd_PollUsb/Bt、电池检测。
 *
 * 事件表 (systemInit 中注册):
 *   led_flow_event     1ms   流水灯
 *   iwdg_feedevent     10ms  看门狗喂狗 (iwdg_init 已注释，但事件仍注册)
 *   usb_connect        10ms  USB 连接状态检测
 *   scan_adc           100ms ADC 采样
 *   scan_ir_reflect    50ms  红外反射采样
 *   usb_receive        0ms   USB 协议帧接收 (空闲 5ms 后触发)
 *   beep               1ms   蜂鸣更新
 *   key_middle_event   1ms   按键扫描
 *   monitor_event      1ms   监控上报触发
 *
 * 新增周期性任务应注册为 EVENT_MANAGER 条目，而非在主循环中忙等。
 *
 * 通信架构: 双通道 + 统一协议
 * ---------------------------
 * 传输层:
 *   USB CDC 虚拟串口    (DRV_COMM_USB, 1KB STX Ymodem 块)
 *   蓝牙 UART5           (DRV_COMM_UART5, 128B SOH Ymodem 块)
 *
 * 应用层协议帧:
 *   格式: 0x5A | sID | oID | len | index | data[256] | crc | 0xA5
 *   解析: 状态机解析，busDataparsing() 按命令字节分发
 *
 * 命令字:
 *   0xB6/0xB9  启动/停止脚本 (toggle)
 *   0xBA        启用监控上报
 *   0xBE        禁用监控上报
 *
 * 文本命令 (行分隔，以 \r\n 结尾):
 *   ymodem              启动 Ymodem 接收 (PikaPython 字节码)
 *   ymodem update fmware 启动固件升级
 *   AT+...              蓝牙 AT 命令透传
 *
 * 监控上报 (JSON Telemetry)
 * -------------------------
 * 周期: USB 1ms, BT 25ms (节流发送)
 * 格式: JSON, 包含设备列表(超声波/触摸/颜色)、ADC(电池/红外)、版本、堆使用率、蓝牙属性、脚本状态
 * 暂停: Ymodem 传输期间通过 set_event_disable("monitor_event") + AppMonitor_SetUploadPaused(1) 双重暂停
 * BT 发送: 单帧上限 248B (DRV_BT_MONITOR_TX_MAX), 门控在 BLUE_STA(PA8) 连接状态
 *
 * PikaPython 运行时
 * -----------------
 * 加载: 通过 Ymodem 接收 .py.o/.py.a 字节码 (magic: 0x0F 'p' 'y' 'o'/'a')
 * 执行: AppPika_Start() -> pikaVM_runByteCodeInconstant() 阻塞运行
 * 停止: 按键 toggle 或 0xB6/0xB9 命令
 * Hook: 每 50 条 VM 指令触发 pika_hook_instruct()，轮询 Monitor_Poll/AppCmd_PollUsb/Bt/电池
 * Sleep: 每 1ms 调用 pika_hook_instruct()，保持监控和命令响应
 * 启动调度: 统一通过 start_py 标志，由 main() 主循环在线程模式调用 AppPika_Start()，
 *           避免在 ISR 上下文中启动 VM
 *
 ***************************************************************************************************
 * 代码结构
 * ========
 *
 * Users/                    APP 入口和应用层
 *   main.c                 main()、systemInit()、Main_Loop_Process()、关机序列
 *   app_cmd.c              命令行处理、协议帧分发、Ymodem 入口、监控暂停控制
 *   app_pika_runtime.c     PikaPython 运行时、Hook、启动/停止逻辑
 *   app_pika_script_flash.c 脚本 Flash 持久化 (CRC32、读写、校验)
 *   app_boot_param.c       启动参数页 (magic + flags)
 *   pika_config.h/c        PikaPython 平台配置 (malloc/free/printf/Hook)
 *   stm32f1xx_it.c         HAL 中断处理 (SysTick 等)
 *   stm32f1xx_hal_conf.h   HAL 配置
 *
 * Drivers/
 *   BSP/                   外设驱动 (ADC, IIC, IR, KEY, LED, SPI, STMFLASH, TIMER, WDG)
 *     drv_flash_storage.c  Flash 配置存储 (BLE 配对、颜色校准)
 *   SYSTEM/                正点原子系统层 (sys, delay, usart)
 *   CMSIS/                 CMSIS 核心 + STM32F1 设备头文件
 *
 * application/             高层功能模块
 *   beep/                  蜂鸣控制
 *   blue/                  蓝牙 BLE (AT 命令、广播数据)
 *   color/                 颜色传感器
 *   matrix/                TM1640 LED 矩阵 + UI
 *   motor/                 电机驱动
 *   touch/                 触摸传感器
 *   ultrasion/             超声波传感器
 *
 * Middlewares/              中间件
 *   protocol/              应用层协议帧解析
 *   monitor/               监控 JSON 构造 + 双通道节流上报
 *   ymodem/                Ymodem 协议 (CRC-16) + drv_comm (双通道 ring buffer) + drv_mem (文件缓冲)
 *   event_manager/         事件调度器
 *   bat_manager/           电池管理 (ADC 采样 + 去抖 + 百分比)
 *   usb/                   USB CDC 虚拟串口
 *   fatfs/                 文件系统
 *   malloc/                内存管理
 *   stubs/                 硬件缺失时的桩实现 (_matrix_stub.c, matrix_port_stub.c)
 *
 * python/                   PikaPython 主机端
 *   main.py                脚本入口
 *   _*.pyi                 模块绑定存根
 *   pikascript-core/       PikaPython 核心 VM (不可修改)
 *   pikascript-lib/        标准库 + 项目定制模块 (_os, motor 等)
 *   pikascript-api/        生成的 API 绑定
 *
 * tools/                    Python 部署工具
 *   lbs_fw_update.py       固件升级 (USB/BT Ymodem)
 *   pika_deploy.py         脚本编译 + 部署
 *   mdk_deploy_prompt.py   Keil After Build 烧录提示
 *   copy_bin_to.py         编译后自动拷贝 .bin
 *
 ***************************************************************************************************
 * 开发约定
 * ========
 * - 类型: 使用 uint8_t/uint16_t/uint32_t 固定宽度类型
 * - 字面量: 0U/1U 后缀对齐 HAL 风格
 * - 忽略返回值: 使用 (void) 显式转换
 * - 注释: 中文注释，UTF-8 编码 (.c/.h)，GBK 编码 (遗留 .txt)
 * - SWD: 保持 SWD 开启用于调试，勿重新启用 JTAG
 * - PikaPython 核心源码 (python/pikascript-core/): 不可修改
 * - 样式: 匹配周围代码的命名、缩进、注释密度
 *
 ***************************************************************************************************
 * 已知问题
 * ========
 * 1. 流水灯少一个灯 (搁置)
 *    现象: 脚本下载失败后流水灯(PC0-PC3)偶尔少点一个，重启后依旧但有时自愈。
 *    怀疑: Bootloader 或硬件接触问题 (不在本仓库范围内)。
 *
 * 2. Ymodem 提前返回导致监控永久停止 (未修复)
 *    app_cmd.c:463 ymodem 命令处理: 先禁用 monitor_event 和暂停上传,
 *    若 DrvComm_Bind 失败则提前返回，事件和暂停标志都不会恢复。
 *    后果: 一次失败的 Ymodem 传输导致 USB/BT 监控永久停止，直到重启。
 *
 ***************************************************************************************************
 * 版本历史
 * ========
 * 2026-07-16: 修复脚本 sleep 时监控停止 + 0xB6 命令启动死机
 * 2026-07-15: 实现 PikaPython 脚本 Flash 持久化
 * 2026-07-15: 新增编译后自动拷贝 .bin 脚本
 * 初始版本: 正点原子 MiniSTM32 V4  HAL 库版本模板
 *
 ***************************************************************************************************
 * 参考资料
 * ========
 * 正点原子: www.alientek.com
 * PikaPython: https://github.com/pikastech/pikapython
 * STM32 HAL: https://www.st.com/en/embedded-software/stm32cube-mcu-mpu-packages.html
 ***********************************************************************************************************
 */