# PikaPython 脚本 Flash 持久化 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现 PikaPython 脚本在内部 Flash 中持久化: 长按关机时保存, 启动时自动装载(不运行), Ymodem 下载仍只写 RAM。

**Architecture:** 新建独立模块 `app_pika_script_flash` (方案 B), 在 Flash 中划出 32KB 单槽区 (0x08060000–0x08067FFF), 用 CRC32 校验完整性, 复用 HAL Flash API 与现有 `drv_flash_storage.c` 风格一致。散列文件拆分为 ER_IROM1/ER_IROM2 预留脚本区。

**Tech Stack:** C (STM32 HAL), Keil µVision MDK-ARM

## Global Constraints

- 脚本区地址: 0x08060000, 大小 32KB, 单槽, 散列文件拆分 ER_IROM1/ER_IROM2 预留
- 保存触发: 仅长按关机 `app_shutdown_sequence()` 内调用
- 启动加载: 仅装载 `AppPika_LoadBytecode()`, 不自动运行
- Ymodem 下载: 只写 RAM, 不触发 Flash 写
- 校验: CRC32, 写前先擦 magic (防写中断)
- 代码风格: 匹配现有 `uint8_t`/`uint32_t` 固定宽度类型, `0U` 后缀, GBK 注释

---
---

### Task 1: 创建头文件 `app_pika_script_flash.h`

**Files:**
- Create: `Users/app_pika_script_flash.h`

**Interfaces:**
- Produces: `AppPikaScriptFlash_LoadToRam()`, `AppPikaScriptFlash_SaveFromRam()`, `AppPikaScriptFlash_HasValid()`, `AppPikaScriptFlash_Invalidate()`
- Produces: `APP_SCRIPT_FLASH_ADDR`, `APP_SCRIPT_FLASH_SIZE`, `APP_SCRIPT_FLASH_MAGIC`, `APP_SCRIPT_FLASH_VERSION`, `APP_SCRIPT_FLASH_HEADER_SIZE`, `APP_SCRIPT_FLASH_MAX_PAYLOAD`

- [ ] **Step 1: 创建头文件**

```c
#ifndef APP_PIKA_SCRIPT_FLASH_H
#define APP_PIKA_SCRIPT_FLASH_H

#include <stdint.h>

/* 脚本持久化区: Flash 0x08060000 ~ 0x08067FFF, 32KB */
#define APP_SCRIPT_FLASH_ADDR       0x08060000U
#define APP_SCRIPT_FLASH_SIZE       (32U * 1024U)
#define APP_SCRIPT_FLASH_MAGIC      0x6F795053U  /* 'SPyo' */
#define APP_SCRIPT_FLASH_VERSION    1U
#define APP_SCRIPT_FLASH_HEADER_SIZE 16U
#define APP_SCRIPT_FLASH_MAX_PAYLOAD (APP_SCRIPT_FLASH_SIZE - APP_SCRIPT_FLASH_HEADER_SIZE - 4U)

/* 启动时检测 Flash 脚本区, 有效则拷贝到 RAM 并装载; 失败返回负值 */
int AppPikaScriptFlash_LoadToRam(void);

/* 关机时把当前 RAM 脚本保存到 Flash; 失败返回负值 */
int AppPikaScriptFlash_SaveFromRam(void);

/* 快速检查 Flash 脚本区是否有有效脚本, 有效返回 1, 无效返回 0 */
int AppPikaScriptFlash_HasValid(void);

/* 擦除 Flash 脚本区, 标记为无效 */
int AppPikaScriptFlash_Invalidate(void);

#endif
```

- [ ] **Step 2: 提交**

```bash
git add Users/app_pika_script_flash.h
git commit -m "feat: 新增 app_pika_script_flash.h 头文件

32KB 单槽脚本持久化区, API 定义: LoadToRam/SaveFromRam/HasValid/Invalidate
"
```

---

### Task 2: 创建实现文件 `app_pika_script_flash.c`

**Files:**
- Create: `Users/app_pika_script_flash.c`

**Interfaces:**
- Consumes: `APP_SCRIPT_FLASH_*` 宏 (Task 1)
- Consumes: `DrvMem_GetFileBuffer()`, `DrvMem_GetFileLength()`, `DrvMem_SetFileLength()` (from `Middlewares/ymodem/drv_mem.h`)
- Consumes: `AppPika_LoadBytecode()` (from `Users/app_pika_runtime.h`)
- Produces: `AppPikaScriptFlash_LoadToRam()`, `AppPikaScriptFlash_SaveFromRam()`, `AppPikaScriptFlash_HasValid()`, `AppPikaScriptFlash_Invalidate()`

- [ ] **Step 1: 编写实现文件**

```c
#include "app_pika_script_flash.h"
#include "app_pika_runtime.h"
#include "drv_mem.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/* 脚本区: 32KB = 16 个 2KB 页 */
#define SCRIPT_FLASH_PAGE_SIZE      0x800U
#define SCRIPT_FLASH_PAGE_COUNT     16U

/* ------------------------------------------------------------------ */
/* Flash 底层操作 (复用 HAL, 与 drv_flash_storage.c 风格一致)         */
/* ------------------------------------------------------------------ */

static int script_flash_erase_pages(uint32_t start_addr, uint32_t page_count)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0U;
    HAL_StatusTypeDef st;

    HAL_FLASH_Unlock();
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = start_addr;
    erase.NbPages = page_count;
    st = HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Lock();
    return (st == HAL_OK) ? 0 : -1;
}

static int script_flash_program_words(uint32_t addr, const uint32_t *src, uint32_t word_cnt)
{
    uint32_t i;
    HAL_StatusTypeDef st;

    HAL_FLASH_Unlock();
    for (i = 0U; i < word_cnt; i++)
    {
        st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + (i * 4U), src[i]);
        if (st != HAL_OK)
        {
            HAL_FLASH_Lock();
            return -1;
        }
    }
    HAL_FLASH_Lock();
    return 0;
}

/* ------------------------------------------------------------------ */
/* CRC32 复用 drv_flash_storage.c 的 crc32_update 算法                */
/* ------------------------------------------------------------------ */

static uint32_t script_flash_crc32(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t j;

    crc = ~crc;
    for (i = 0U; i < len; i++)
    {
        crc ^= data[i];
        for (j = 0U; j < 8U; j++)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (crc >> 1U) ^ 0xEDB88320U;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }
    return ~crc;
}

/* ------------------------------------------------------------------ */
/* 对外 API                                                           */
/* ------------------------------------------------------------------ */

int AppPikaScriptFlash_HasValid(void)
{
    const uint32_t *flash = (const uint32_t *)APP_SCRIPT_FLASH_ADDR;
    uint32_t magic;
    uint32_t version;
    uint32_t length;
    uint32_t total_len;
    uint32_t pad;
    uint32_t crc_stored;
    uint32_t crc_calc;

    magic = flash[0];
    if (magic != APP_SCRIPT_FLASH_MAGIC)
    {
        return 0;
    }

    version = flash[1];
    if (version != APP_SCRIPT_FLASH_VERSION)
    {
        return 0;
    }

    length = flash[2];
    if (length == 0U || length > APP_SCRIPT_FLASH_MAX_PAYLOAD)
    {
        return 0;
    }

    /* CRC32 覆盖: header + script_data + padding */
    total_len = APP_SCRIPT_FLASH_HEADER_SIZE + length;
    pad = (4U - (total_len % 4U)) % 4U;
    total_len += pad;

    crc_stored = *(const uint32_t *)(APP_SCRIPT_FLASH_ADDR + total_len);
    crc_calc = script_flash_crc32(0U, (const uint8_t *)APP_SCRIPT_FLASH_ADDR, total_len);

    return (crc_stored == crc_calc) ? 1 : 0;
}

int AppPikaScriptFlash_LoadToRam(void)
{
    const uint32_t *flash = (const uint32_t *)APP_SCRIPT_FLASH_ADDR;
    uint32_t length;
    const uint8_t *script_data;

    if (AppPikaScriptFlash_HasValid() == 0)
    {
        return -1;
    }

    length = flash[2];
    script_data = (const uint8_t *)(APP_SCRIPT_FLASH_ADDR + APP_SCRIPT_FLASH_HEADER_SIZE);

    /* 拷贝到 RAM 缓冲 */
    if (length > DrvMem_GetFileBufferSize())
    {
        return -2;
    }
    memcpy(DrvMem_GetFileBuffer(), script_data, length);
    DrvMem_SetFileLength(length);

    /* 装载到 PikaVM; 返回 READY 状态, 不自动运行 */
    return AppPika_LoadBytecode(DrvMem_GetFileBuffer(), length);
}

int AppPikaScriptFlash_SaveFromRam(void)
{
    uint32_t length;
    const uint8_t *ram;
    uint32_t header[4];
    uint32_t crc;
    uint32_t i;
    uint32_t word_cnt;
    uint32_t total_data_len;
    uint32_t pad;
    uint32_t w;
    uint32_t off;
    uint32_t remain;

    length = DrvMem_GetFileLength();
    ram = DrvMem_GetFileBuffer();

    if (length == 0U || ram == NULL)
    {
        return -1;
    }
    if (length > APP_SCRIPT_FLASH_MAX_PAYLOAD)
    {
        return -1;
    }

    /* 1. 擦除整个脚本区 */
    if (script_flash_erase_pages(APP_SCRIPT_FLASH_ADDR, SCRIPT_FLASH_PAGE_COUNT) != 0)
    {
        return -2;
    }

    /* 2. 写 header (4 个 word) */
    header[0] = APP_SCRIPT_FLASH_MAGIC;
    header[1] = APP_SCRIPT_FLASH_VERSION;
    header[2] = length;
    header[3] = 0U;
    if (script_flash_program_words(APP_SCRIPT_FLASH_ADDR, header, 4U) != 0)
    {
        return -3;
    }

    /* 3. 写 script_data, word 对齐, 不足 4 字节用 0x1A 填充 */
    word_cnt = (length + 3U) / 4U;
    for (i = 0U; i < word_cnt; i++)
    {
        off = i * 4U;
        remain = length - off;
        if (remain >= 4U)
        {
            w = ((uint32_t)ram[off])
              | ((uint32_t)ram[off + 1U] << 8U)
              | ((uint32_t)ram[off + 2U] << 16U)
              | ((uint32_t)ram[off + 3U] << 24U);
        }
        else
        {
            w = 0U;
            for (uint32_t j = 0U; j < remain; j++)
            {
                ((uint8_t *)&w)[j] = ram[off + j];
            }
            for (uint32_t j = remain; j < 4U; j++)
            {
                ((uint8_t *)&w)[j] = 0x1AU;
            }
        }
        if (script_flash_program_words(APP_SCRIPT_FLASH_ADDR + APP_SCRIPT_FLASH_HEADER_SIZE + off, &w, 1U) != 0)
        {
            return -4;
        }
    }

    /* 4. 计算 CRC32: header + script_data + padding */
    total_data_len = APP_SCRIPT_FLASH_HEADER_SIZE + length;
    pad = (4U - (total_data_len % 4U)) % 4U;
    crc = script_flash_crc32(0U, (const uint8_t *)header, sizeof(header));
    crc = script_flash_crc32(crc, ram, length);
    if (pad > 0U)
    {
        static const uint8_t pad_bytes[3] = {0x1AU, 0x1AU, 0x1AU};
        crc = script_flash_crc32(crc, pad_bytes, pad);
    }

    /* 5. 写 CRC32 */
    return script_flash_program_words(
        APP_SCRIPT_FLASH_ADDR + APP_SCRIPT_FLASH_HEADER_SIZE + word_cnt * 4U, &crc, 1U);
}

int AppPikaScriptFlash_Invalidate(void)
{
    return script_flash_erase_pages(APP_SCRIPT_FLASH_ADDR, 1U);
}
```

- [ ] **Step 2: 提交**

```bash
git add Users/app_pika_script_flash.c
git commit -m "feat: 实现 app_pika_script_flash.c 脚本 Flash 持久化

- HasValid: magic + version + length + CRC32 校验
- LoadToRam: 拷贝到 DrvMem → AppPika_LoadBytecode
- SaveFromRam: 先擦 16 页 → 写 header → 写 script_data → 写 CRC32
- Invalidate: 擦第一页清 magic
- 写前擦 magic 确保断电安全: 要么全写入, 要么什么都没
"
```

---

### Task 3: 修改 `main.c` — 加入启动加载和关机保存

**Files:**
- Modify: `Users/main.c`

**Interfaces:**
- Consumes: `AppPikaScriptFlash_LoadToRam()`, `AppPikaScriptFlash_SaveFromRam()` (Task 2)

- [ ] **Step 1: 在 `main.c` 顶部新增 include**

```c
// 在现有 #include "app_pika_runtime.h" 之后添加:
#include "app_pika_script_flash.h"
```

目标位置: [Users/main.c:7](Users/main.c#L7) 之后 (第 8 行)

- [ ] **Step 2: 在 `systemInit()` 末尾新增启动加载**

在 `blue_init();` 之后、`AppCmd_SyncBtFromModule();` 之前插入:

```c
    blue_init();
    AppPikaScriptFlash_LoadToRam();   /* 若 Flash 有有效脚本, 装载到 RAM (不运行) */
    AppCmd_SyncBtFromModule();
```

目标位置: [Users/main.c:204-205](Users/main.c#L204) 之间

- [ ] **Step 3: 在 `app_shutdown_sequence()` 开头新增关机保存**

在 `beep_stop();` 之后、`DrvLed_SetFlowEnable(0U);` 之前插入:

```c
    beep_stop();
    AppPikaScriptFlash_SaveFromRam();   /* 关机前保存当前脚本到 Flash */
    DrvLed_SetFlowEnable(0U);
```

目标位置: [Users/main.c:74-75](Users/main.c#L74) 之间

- [ ] **Step 4: 提交**

```bash
git add Users/main.c
git commit -m "feat: main.c 接入脚本 Flash 持久化

- systemInit: 启动时自动装载 Flash 脚本 (不运行)
- app_shutdown_sequence: 长按关机前保存脚本
"
```

---

### Task 4: 修改 Keil 工程 — 加入新文件

**Files:**
- Modify: `Projects/MDK-ARM/atk_f103.uvprojx`

- [ ] **Step 1: 在 Keil 工程中添加 `app_pika_script_flash.c`**

在 Keil µVision 中打开工程, 在 `Project` 窗口右键 `Users` 组 → `Add Existing Files to Group 'Users'` → 选择 `Users/app_pika_script_flash.c`。

或在 `.uvprojx` XML 中手动添加: 在 `<GroupName>Users</GroupName>` 对应的 `<Files>` 节中添加:

```xml
<File>
  <FileName>app_pika_script_flash.c</FileName>
  <FileType>1</FileType>
  <FilePath>.\Users\app_pika_script_flash.c</FilePath>
</File>
```

- [ ] **Step 2: 提交**

```bash
git add Projects/MDK-ARM/atk_f103.uvprojx
git commit -m "build: 将 app_pika_script_flash.c 加入 Keil 工程 Users 组"
```

---

### Task 5: 构建验证

**Files:**
- 无新建/修改, 验证 Task 1-4 产物

- [ ] **Step 1: 在 Keil µVision 中 Build**

打开 `Projects/MDK-ARM/atk_f103.uvprojx`, 选择 target `SparkAi`, 按 F7 构建。

预期: 0 errors, 0 warnings.

- [ ] **Step 2: 检查 `.map` 文件确认脚本区未被占用**

在 `Output/atk_f103.map` 中搜索 `0x08040000`, 确认:
- 没有代码/数据段落在此地址
- 脚本区 (0x08040000–0x08047FFF) 与用户配置页 (0x08078000) 之间无冲突

- [ ] **Step 3: 检查编译产物大小**

确认 `atk_f103.bin` 大小 < 0x08040000 - 0x08008000 = 224KB。

- [ ] **Step 4: 提交 (如有 map 变更)**

```bash
# 如果 map 文件被跟踪且已变更:
git add Output/atk_f103.map
git commit -m "build: 验证脚本 Flash 持久化编译通过, 内存布局无冲突"
```

---

### 自检清单

- [ ] Task 1: 头文件定义完整 (地址/大小/魔数/API)
- [ ] Task 2: 实现覆盖所有 API, CRC32 正确, Flash 写前擦 magic
- [ ] Task 3: main.c 3 处改动 (include + systemInit + shutdown)
- [ ] Task 4: Keil 工程包含新文件
- [ ] Task 5: 编译通过, map 无冲突
- [ ] 验证逻辑: 启动加载 → 不自动运行; 关机保存 → 只存当前 RAM 脚本; Ymodem → 只写 RAM