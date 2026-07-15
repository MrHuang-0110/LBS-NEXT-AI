# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Embedded firmware for an STM32F1-class microcontroller (target device **APM32E103RE**, Cortex-M3, 512 KB Flash @ `0x08000000`, 128 KB RAM), built with Keil µVision (MDK-ARM) and the STM32 HAL. The board is a small robot/teaching device (motors, color sensor, LED matrix, ultrasonic, touch, beep, BLE) that runs **PikaPython** scripts downloaded at runtime. Text files (e.g. `readme.txt`) are GBK-encoded Chinese.

This is the **APP** image: it does not boot from address 0. A separate bootloader lives in the first 32 KB of Flash; the APP is linked at `0x08008000` and relocates its vector table on entry (`sys_nvic_set_vector_table(FLASH_BASE, 0x8000)` in [Users/main.c](Users/main.c)). Keep this memory layout in mind when touching linker/flash code.

## Build & deploy

There is no Makefile, no test runner, no linter. Builds happen in Keil: open [Projects/MDK-ARM/atk_f103.uvprojx](Projects/MDK-ARM/atk_f103.uvprojx) (target name `SparkAi`) and build. Output goes to [Output/](Output/) (`atk_f103.axf`, `atk_f103.bin`).

**Firmware deploy (APP .bin → device, via bootloader Ymodem):** the Keil *After Build* step runs [tools/mdk_deploy_prompt.py](tools/mdk_deploy_prompt.py), which converts the `.axf` to `.bin` and pops a dialog asking whether to upgrade over USB. The port is read from [tools/deploy_port.txt](tools/deploy_port.txt) (currently `COM30`). From a shell:

```
python tools/lbs_fw_update.py -p COM30 -f Output/atk_f103.bin            # USB CDC, 1KB STX blocks
python tools/lbs_fw_update.py -p COM7 --bt -f Output/atk_f103.bin        # Bluetooth UART, 128B SOH blocks
```

**PikaPython script deploy (compile .py → bytecode → device):** uses the PikaCompiler (`rust-msc-latest-win10.exe`, bundled in `python/` and `tools/`). The script entry is [python/main.py](python/main.py); module bindings are the `_*.pyi` stubs in `python/`.

```
python tools/pika_deploy.py my_app.py -p COM30                  # compile + deploy over USB
python tools/pika_deploy.py my_app.py -p COM7 --bt              # compile + deploy over Bluetooth
python tools/pika_deploy.py my_app.py --compile-only            # compile, no download
python tools/lbs_fw_update.py deploy main.py -p COM30           # equivalent wrapper
```

`pyserial` is required for all deploy scripts (`pip install pyserial`).

`keilkill.bat` cleans Keil intermediate artifacts; [postbuild.bat](postbuild.bat) is a legacy shim that now just forwards to `mdk_deploy_prompt.py`.

## Architecture

### Two run loops, one scheduler
The APP is **cooperative/event-driven** — there is no RTOS. [Users/main.c](Users/main.c) `systemInit()` registers a static table of `EVENT_MANAGER` entries and the TIM6 ISR (`btim_timx_int`) drives `event_schedlucer()` which dispatches the registered callbacks on their millisecond thresholds (LED flow, IWDG feed, USB connect, ADC sampling, IR reflect sampling, USB receive, beep, key, monitor). The `main()` while-loop additionally polls `Monitor_Poll()`, `AppCmd_PollUsb/Bt()`, and battery checks every iteration. New periodic work should be added as an `EVENT_MANAGER` entry rather than a busy-loop.

### PikaPython runtime
[Users/app_pika_runtime.c](Users/app_pika_runtime.c) loads compiled PikaPython bytecode (`.py.o` / `.py.a`, magic `0x0F 'p' 'y' 'o'/'a'`) received over Ymodem and executes it via PikaVM. To keep the cooperative scheduler alive while a script runs, `pika_hook_instruct()` is invoked every N VM instructions (`PIKA_INSTRUCT_HOOK_ENABLE` in [Users/pika_config.h](Users/pika_config.h)) and polls monitor/cmd/battery from inside the script. The script can be started/stopped by protocol command (e.g. `0xB6`/`0xB9`) or the key toggle. `start_py`, `__pikaMain`, and `pikaModules_py_a` are shared globals — don't rename them without updating the PikaScript-generated bindings under `python/pikascript-api/`.

### Communication: two channels, one protocol
- **Transports** ([Middlewares/ymodem/drv_comm.c](Middlewares/ymodem/drv_comm.c)): USB CDC virtual serial (`DRV_COMM_USB`) and Bluetooth UART (`DRV_COMM_UART5`, `application/blue/`). Each has its own ring buffer and Ymodem block size (USB = 1 KB STX, BT = 128 B SOH).
- **App-level frame protocol** ([Middlewares/protocol/](Middlewares/protocol/)): fixed-format frames `0x5A | sID | oID | len | index | data[256] | crc | 0xA5`, parsed by a state machine. `busDataparsing()` in [Users/app_cmd.c](Users/app_cmd.c) dispatches frames by command byte.
- **Monitor** ([Middlewares/monitor/](Middlewares/monitor/)): serializes registered `MonitorItem`s to JSON (`json-maker.c`) and uploads telemetry over USB/BT. Upload can be paused during Ymodem (`AppMonitor_SetUploadPaused`).
- **Ymodem** ([Middlewares/ymodem/drv_ymodem.c](Middlewares/ymodem/drv_ymodem.c)): CRC-16, used both for APP firmware upgrades (handled by the bootloader) and for pushing PikaPython bytecode into `DrvMem`'s file buffer.

### Flash memory map (do not reorder casually)
- `0x08000000` — bootloader (not in this repo)
- `0x08008000` — APP image start; vector table relocated here
- `0x08007800` — boot parameter page (`APP_BOOT_MAGIC` + flags, e.g. `APP_BOOT_FLAG_UPDATE_REQ` to ask the bootloader to upgrade). See [Users/app_boot_param.c](Users/app_boot_param.c).
- `0x08078000` — user config page (last 2 KB of Flash): BLE pairing state and color-sensor calibration, keyed by magic `PKCL` / `ECB2`. See [Drivers/BSP/drv_flash_storage.c](Drivers/BSP/drv_flash_storage.c).

### Code layout
- [Users/](Users/) — APP entry (`main.c`), `app_cmd.c` (USB/BT command line + frame dispatch), `app_pika_runtime.c`, boot-param and PikaConfig. `stm32f1xx_it.c` / `stm32f1xx_hal_conf.h` are the HAL IT/config glue.
- [Drivers/BSP/](Drivers/BSP/) — peripheral drivers (ADC, IIC, IR, KEY, LED, SPI, STMFLASH, TIMER, WDG) + `drv_flash_storage`.
- [Drivers/SYSTEM/](Drivers/SYSTEM/) — Alientek system layer (clock/delay/sys/usart). Include paths use `./SYSTEM/...` relative prefixes.
- [application/](application/) — higher-level feature modules: `beep`, `blue` (BLE), `color`, `matrix` (TM1640 LED matrix + UI), `motor`, `touch`, `ultrasion`.
- [Middlewares/](Middlewares/) — `protocol`, `monitor`, `ymodem` (+`drv_comm`/`drv_mem`), `event_manager`, `bat_manager`, `deviceIdentify`, `fatfs`, `file_manager`/`lbs_file_manager`, `w25q80x`, `usb`, `RTC`, `malloc`, `stubs`.
- [Middlewares/stubs/](Middlewares/stubs/) — `_matrix_stub.c` / `matrix_port_stub.c` are linked when the matrix hardware is absent; do not assume `matrix_port.c` is always the active implementation.
- [python/](python/) — PikaPython host side: `main.py`, `*.pyi` binding stubs, and `pikascript-core`/`pikascript-lib`/`pikascript-api` (the generated API headers including `PikaMain.h`).
- [tools/](tools/) — Python deploy/upgrade tooling.

### Conventions
- All HAL/Driver code uses `uint8_t/uint32_t` fixed-width types, `0U`/`1U` suffixes on integer literals, and `(void)` casts on intentionally-ignored return values — match this style.
- Comments are mixed Chinese/English; new code in this repo uses Chinese comments. Preserve encoding (GBK for legacy `.txt`, UTF-8 for `.c/.h` and `.py`).
- `disable_jtag_enable_swd()` in `main.c` keeps SWD alive for Keil debugging — do not re-enable full JTAG or you lose the debug probe.

<!-- project-memory:start -->
会话开始时，先调用 mcp__memory__read_graph 读取本项目记忆再开始工作。若因 MCP 服务/包缺失失败，按 project-memory skill 的 setup Step 0 自愈安装后重试。
<!-- project-memory:end -->

