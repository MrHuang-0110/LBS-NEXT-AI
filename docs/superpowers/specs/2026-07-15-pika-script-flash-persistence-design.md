# PikaPython 脚本 Flash 持久化设计

**日期**: 2026-07-15
**状态**: 设计中
**关联**: 方案 B — 新建独立模块 `app_pika_script_flash`

---

## 1. 背景与动机

当前 PikaPython 脚本通过 Ymodem 下载到 RAM 缓冲 (`DrvMem`, 32KB), 设备断电后丢失。
每次上电需要重新下载。本设计实现:

- 正常关机时自动把 RAM 中的脚本写入内部 Flash 持久化
- 下次启动时检测 Flash 脚本区, 有效则自动装载到 RAM (不自动运行)
- 日常 Ymodem 下载仍只操作 RAM, 不动 Flash

---

## 2. 核心约束

| 约束 | 值 |
|---|---|
| 脚本始终在 RAM 执行 | PikaVM 运行 `DrvMem_GetFileBuffer()` 指向的数据 |
| 保存触发 | 仅长按中键关机 (`app_shutdown_sequence`) |
| 启动加载 | 只装载 `AppPika_LoadBytecode`, 不自动运行 |
| Ymodem 下载 | 只写 RAM, 不触发 Flash 写 |
| Flash 脚本区 | 单槽 32KB, 0x08060000–0x08067FFF |
| 校验 | CRC32 (复用 `drv_flash_storage.c` 的 `crc32_update` 算法) |
| MCU / Flash | APM32E103RE, 512KB Flash, 页大小 2KB |

---

## 3. Flash 内存布局

```
0x08000000 +--------------------+
           | Bootloader (32KB)  | 不在本仓库
0x08008000 +--------------------+
           | APP 代码 (ER_IROM1) | 本仓库, 352KB
0x08060000 +--------------------+  ← APP_SCRIPT_FLASH_ADDR (修正: 避开 APP 代码)
           | 脚本持久化区(32KB) | 新增
           |  - header (16B)    |
           |  - script_data     |
           |  - pad + crc32     |
0x08068000 +--------------------+
           | APP 代码续 (ER_IROM2)| 溢出区, 64KB
0x08078000 +--------------------+
           | 用户配置 (2KB)     | 颜色校准 + BT 配对 (drv_flash_storage.c)
0x08080000 +--------------------+
```

与现有区域不冲突: 脚本区 0x08060000–0x08067FFF 通过散列文件拆分 ER_IROM1/ER_IROM2 预留, 链接器不会将代码放入此区。

---

## 4. 脚本区数据格式

单槽, 总 32KB:

```
偏移      大小    字段
0x0000    4B     magic      = 0x6F795053 ('SPyo' 的 little-endian)
0x0004    4B     version    = 1 (格式版本)
0x0008    4B     length     = 实际脚本字节数, 含 .py.o 魔数
0x000C    4B     reserved   = 0 (保留)
0x0010    N      script_data = PikaPython 字节码 (.py.o / .py.a)
...       填充   0x1A 到 4 字节对齐
末尾 4B   crc32   = CRC32(magic + version + length + reserved + script_data + padding)
```

校验流程:

1. 读 magic, 不是 `0x6F795053` → 无效
2. 读 length, 0 或 > `SCRIPT_FLASH_MAX_PAYLOAD` → 无效
3. 读 CRC32, 重新计算比对 → 不匹配则无效
4. 全部通过 → 有效

---

## 5. 新增文件

### `Users/app_pika_script_flash.h`

```c
#ifndef APP_PIKA_SCRIPT_FLASH_H
#define APP_PIKA_SCRIPT_FLASH_H

#include <stdint.h>

/* 脚本持久化 Flash 区 */
#define APP_SCRIPT_FLASH_ADDR       0x08060000U
#define APP_SCRIPT_FLASH_SIZE       (32U * 1024U)
#define APP_SCRIPT_FLASH_MAGIC      0x6F795053U  /* 'SPyo' */
#define APP_SCRIPT_FLASH_VERSION    1U
#define APP_SCRIPT_FLASH_HEADER_SIZE 16U
#define APP_SCRIPT_FLASH_MAX_PAYLOAD (APP_SCRIPT_FLASH_SIZE - APP_SCRIPT_FLASH_HEADER_SIZE - 4U)

/* 加载 Flash 脚本到 RAM, 并调用 AppPika_LoadBytecode; 失败返回负值 */
int AppPikaScriptFlash_LoadToRam(void);

/* 把当前 RAM 脚本保存到 Flash; 失败返回负值 */
int AppPikaScriptFlash_SaveFromRam(void);

/* 快速检查 Flash 脚本区是否有有效脚本 */
int AppPikaScriptFlash_HasValid(void);

/* 擦除 Flash 脚本区 magic, 标记为无效 */
int AppPikaScriptFlash_Invalidate(void);

#endif
```

### `Users/app_pika_script_flash.c`

实现以下内部函数 + 对外 API:

- `script_flash_erase_sector()` — 用 HAL_FLASHEx_Erase 擦除 script 区
- `script_flash_program_words()` — 用 HAL_FLASH_Program 写 word
- `script_flash_compute_crc32()` — 复用 `crc32_update` 算法
- `AppPikaScriptFlash_HasValid()` — 读 header 校验 magic + length + CRC32
- `AppPikaScriptFlash_LoadToRam()` — `HasValid()` → 拷贝到 `DrvMem` → `AppPika_LoadBytecode()`
- `AppPikaScriptFlash_SaveFromRam()` — 检查 RAM 有无脚本 → `Invalidate()` → 写 header + data + CRC32
- `AppPikaScriptFlash_Invalidate()` — 擦除整个脚本区

---

## 6. 修改现有代码

### `Users/main.c`

**新增 `#include`**:
```c
#include "app_pika_script_flash.h"
```

**`systemInit()` 末尾新增**(在 `blue_init()` 之后、`AppCmd_SyncBtFromModule()` 之前):
```c
blue_init();
AppPikaScriptFlash_LoadToRam();   // 忽略返回值; 无效/失败时无副作用
AppCmd_SyncBtFromModule();
```

**`app_shutdown_sequence()` 开头新增**:
```c
static void app_shutdown_sequence(void)
{
    beep_stop();
    AppPikaScriptFlash_SaveFromRam();   // 新增: 保存当前脚本到 Flash
    DrvLed_SetFlowEnable(0U);
    DrvLed_PlayShutdownAnimationBlocking();
    ...
}
```

### `Projects/MDK-ARM/atk_f103.uvprojx`

在 `Users` 组添加文件:
```
Users/app_pika_script_flash.c
```

无链接脚本改动。

### 其他文件

- `Users/app_pika_runtime.c` — 无改动 (LoadToRam 内部调用已有 API)
- `Users/app_cmd.c` — 无改动 (Ymodem 路径不动 Flash)
- `Middlewares/ymodem/drv_mem.c` — 无改动

---

## 7. 错误处理矩阵

| 场景 | 行为 |
|---|---|
| Flash 脚本区全空 (0xFF) | `HasValid()` 返回 0, `LoadToRam()` 返回 -1, 无副作用 |
| magic 不对 | 同上 (旧版本/误写/未初始化) |
| magic 对但 CRC 不匹配 | 同上 (写中断或数据损坏) |
| magic 对但 length 超限 | 同上 (异常数据) |
| 关机保存时 RAM 无脚本 | `SaveFromRam()` 返回 -1, 不擦除, 不写 |
| 关机保存时脚本超过 Flash 区容量 | `SaveFromRam()` 返回 -1, 不擦除, 不写 |
| 关机保存时 Flash 写失败 | 返回 -1; 旧 magic 已被清, 下次启动判定无脚本 |
| 关机保存中途断电 | Flash 区半截数据; 下次启动 magic 无效 → 不加载 |
| Ymodem 下载失败 | 不动 Flash 区, 只清 RAM 缓冲 |
| Ymodem 下载成功 | 只写 RAM, 不动 Flash 区 |

**写流程的关键设计**: `SaveFromRam()` 先擦除旧 magic → 再写新数据。如果中途断电, magic 已无效, 下次启动不会加载损坏的脚本。确保要么全写入, 要么什么都没有。

---

## 8. 交互序列

### 首次使用 (Flash 无脚本)

```
上电 → systemInit() → LoadToRam() → magic 无效, 返回 -1 → 无脚本
     → 用户通过 USB/BT 下载脚本 → Ymodem → RAM → AppPika_LoadBytecode ✓
     → 用户中键运行脚本
     → 用户长按关机 → SaveFromRam() → 写 Flash ✓
```

### 后续启动 (Flash 有脚本)

```
上电 → systemInit() → LoadToRam() → magic 有效, CRC 匹配
     → 拷贝到 DrvMem → AppPika_LoadBytecode() → 状态 READY
     → 用户中键启动 (或帧命令 0xB6/0xB9)
```

### 更新脚本

```
下载新脚本 → Ymodem → RAM 覆盖 → AppPika_LoadBytecode ✓
     → Flash 仍是旧脚本 (未触发保存)
     → 用户长按关机 → SaveFromRam() → 新脚本覆盖 Flash 旧脚本 ✓
     → 如果用户不关机直接断电 → 新脚本丢失, Flash 保留旧脚本
```

---

## 9. 自检清单

- [ ] 新增 `Users/app_pika_script_flash.h` 和 `.c` 文件
- [ ] 修改 `Users/main.c` (3 处: include + systemInit + app_shutdown_sequence)
- [ ] 修改 `Projects/MDK-ARM/atk_f103.uvprojx` (加入新文件)
- [ ] 验证启动加载: Flash 空 → 无脚本; Flash 有效 → 装载到 READY
- [ ] 验证关机保存: 有脚本 → 写入; 无脚本 → 跳过
- [ ] 验证 Ymodem 下载: 只写 RAM, 不写 Flash
- [ ] 验证 CRC32 与写入中断: 断电后 magic 无效 → 不加载损坏数据