/**
 * test_script_flash.c — PikaPython 脚本 Flash 持久化 单元测试
 *
 * 编译: gcc -Wall -Wextra -std=c99 -o test_script_flash test_script_flash.c && ./test_script_flash
 * 目标: 验证 CRC32 算法、HasValid 校验逻辑、地址计算一致性
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* 复制生产代码中的常量和 CRC32 (与 app_pika_script_flash.c 一致)     */
/* ------------------------------------------------------------------ */

#define APP_SCRIPT_FLASH_ADDR       0x08060000U
#define APP_SCRIPT_FLASH_SIZE       (32U * 1024U)
#define APP_SCRIPT_FLASH_MAGIC      0x6F795053U
#define APP_SCRIPT_FLASH_VERSION    1U
#define APP_SCRIPT_FLASH_HEADER_SIZE 16U
#define APP_SCRIPT_FLASH_MAX_PAYLOAD (APP_SCRIPT_FLASH_SIZE - APP_SCRIPT_FLASH_HEADER_SIZE - 4U)

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
/* 模拟 Flash 缓冲 (主机端, 初始化为 0xFF)                            */
/* ------------------------------------------------------------------ */

static uint8_t mock_flash[APP_SCRIPT_FLASH_SIZE];

static void mock_flash_reset(void)
{
    memset(mock_flash, 0xFF, sizeof(mock_flash));
}

/* ------------------------------------------------------------------ */
/* 复制 HasValid 逻辑 (读 mock_flash 而非真实 Flash)                   */
/* ------------------------------------------------------------------ */

static int mock_has_valid(void)
{
    const uint32_t *flash = (const uint32_t *)mock_flash;
    uint32_t magic;
    uint32_t version;
    uint32_t length;
    uint32_t total_len;
    uint32_t pad;
    uint32_t crc_stored;
    uint32_t crc_calc;

    magic = flash[0];
    if (magic != APP_SCRIPT_FLASH_MAGIC) return 0;

    version = flash[1];
    if (version != APP_SCRIPT_FLASH_VERSION) return 0;

    length = flash[2];
    if (length == 0U || length > APP_SCRIPT_FLASH_MAX_PAYLOAD) return 0;

    total_len = APP_SCRIPT_FLASH_HEADER_SIZE + length;
    pad = (4U - (total_len % 4U)) % 4U;
    total_len += pad;

    crc_stored = *(const uint32_t *)(mock_flash + total_len);
    crc_calc = script_flash_crc32(0U, mock_flash, total_len);

    return (crc_stored == crc_calc) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* 辅助: 构建有效脚本到 mock_flash                                     */
/* ------------------------------------------------------------------ */

static void build_valid_script(const uint8_t *data, uint32_t len)
{
    uint32_t total_len;
    uint32_t pad;
    uint32_t crc;
    uint32_t *header;

    mock_flash_reset();
    header = (uint32_t *)mock_flash;
    header[0] = APP_SCRIPT_FLASH_MAGIC;
    header[1] = APP_SCRIPT_FLASH_VERSION;
    header[2] = len;
    header[3] = 0U;

    memcpy(mock_flash + APP_SCRIPT_FLASH_HEADER_SIZE, data, len);

    total_len = APP_SCRIPT_FLASH_HEADER_SIZE + len;
    pad = (4U - (total_len % 4U)) % 4U;
    if (pad > 0U)
    {
        memset(mock_flash + total_len, 0x1AU, pad);
    }
    total_len += pad;

    crc = script_flash_crc32(0U, mock_flash, total_len);
    memcpy(mock_flash + total_len, &crc, sizeof(crc));
}

/* ------------------------------------------------------------------ */
/* 测试用例                                                           */
/* ------------------------------------------------------------------ */

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) do { \
    printf("  %-50s ", name); \
} while (0)

#define PASS() do { \
    printf("PASS\n"); g_pass++; \
} while (0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); g_fail++; \
} while (0)

/* Test 1: CRC32 已知值 — 空数据 */
static void test_crc32_empty(void)
{
    TEST("CRC32 of empty data");
    uint32_t crc = script_flash_crc32(0U, NULL, 0U);
    if (crc == 0U) { PASS(); } else { FAIL("empty CRC32 should be 0"); }
}

/* Test 2: CRC32 已知值 — "123456789" (标准测试向量) */
static void test_crc32_known_vector(void)
{
    TEST("CRC32 of '123456789' (standard test vector)");
    const uint8_t data[] = "123456789";
    uint32_t crc = script_flash_crc32(0U, data, 9U);
    /* 标准 CRC32 对 "123456789" 的结果是 0xCBF43926 */
    if (crc == 0xCBF43926U) { PASS(); } else { FAIL("expected 0xCBF43926"); }
}

/* Test 3: CRC32 链式调用正确 */
static void test_crc32_chaining(void)
{
    TEST("CRC32 chaining: a+b == ab");
    const uint8_t a[] = "hello";
    const uint8_t b[] = "world";
    const uint8_t ab[] = "helloworld";
    uint32_t crc_chain = script_flash_crc32(0U, a, 5U);
    crc_chain = script_flash_crc32(crc_chain, b, 5U);
    uint32_t crc_full = script_flash_crc32(0U, ab, 10U);
    if (crc_chain == crc_full) { PASS(); } else { FAIL("chained CRC32 != full CRC32"); }
}

/* Test 4: 空 Flash (全 0xFF) → HasValid 返回 0 */
static void test_has_valid_empty_flash(void)
{
    TEST("HasValid on empty (0xFF) Flash returns 0");
    mock_flash_reset();
    if (mock_has_valid() == 0) { PASS(); } else { FAIL("should return 0 on empty flash"); }
}

/* Test 5: 有效脚本 → HasValid 返回 1 */
static void test_has_valid_good_script(void)
{
    TEST("HasValid on valid script returns 1");
    const uint8_t data[] = {0x0F, 'p', 'y', 'o', 0x00, 0x01, 0x02, 0x03};
    build_valid_script(data, sizeof(data));
    if (mock_has_valid() == 1) { PASS(); } else { FAIL("should return 1 on valid script"); }
}

/* Test 6: CRC32 被破坏 → HasValid 返回 0 */
static void test_has_valid_corrupt_crc(void)
{
    TEST("HasValid returns 0 when CRC32 corrupted");
    const uint8_t data[] = {0x0F, 'p', 'y', 'o', 0xAA, 0xBB};
    build_valid_script(data, sizeof(data));
    /* 翻转 CRC32 的第一个字节 */
    uint32_t total_len = APP_SCRIPT_FLASH_HEADER_SIZE + (uint32_t)sizeof(data);
    uint32_t pad = (4U - (total_len % 4U)) % 4U;
    mock_flash[total_len + pad] ^= 0xFFU;
    if (mock_has_valid() == 0) { PASS(); } else { FAIL("should return 0 when CRC32 corrupted"); }
}

/* Test 7: magic 错误 → HasValid 返回 0 */
static void test_has_valid_bad_magic(void)
{
    TEST("HasValid returns 0 when magic is wrong");
    const uint8_t data[] = {0x0F, 'p', 'y', 'o'};
    build_valid_script(data, sizeof(data));
    mock_flash[0] = 0x00;  /* 破坏 magic */
    if (mock_has_valid() == 0) { PASS(); } else { FAIL("should return 0 with bad magic"); }
}

/* Test 8: version 不匹配 → HasValid 返回 0 */
static void test_has_valid_bad_version(void)
{
    TEST("HasValid returns 0 when version mismatches");
    const uint8_t data[] = {0x0F, 'p', 'y', 'o'};
    build_valid_script(data, sizeof(data));
    mock_flash[4] = 0xFF;  /* 破坏 version */
    if (mock_has_valid() == 0) { PASS(); } else { FAIL("should return 0 with bad version"); }
}

/* Test 9: length=0 → HasValid 返回 0 */
static void test_has_valid_zero_length(void)
{
    TEST("HasValid returns 0 when length=0");
    const uint8_t data[] = {0xAA};  /* 任意数据 */
    build_valid_script(data, sizeof(data));
    /* 手动把 length 字段改成 0 */
    ((uint32_t *)mock_flash)[2] = 0U;
    if (mock_has_valid() == 0) { PASS(); } else { FAIL("should return 0 when length=0"); }
}

/* Test 10: 最大 payload → HasValid 返回 1 */
static void test_has_valid_max_payload(void)
{
    TEST("HasValid with max payload (32748 bytes)");
    uint8_t *big_data = (uint8_t *)malloc(APP_SCRIPT_FLASH_MAX_PAYLOAD);
    if (big_data == NULL) { FAIL("malloc failed"); return; }
    for (uint32_t i = 0U; i < APP_SCRIPT_FLASH_MAX_PAYLOAD; i++)
    {
        big_data[i] = (uint8_t)(i & 0xFFU);
    }
    build_valid_script(big_data, APP_SCRIPT_FLASH_MAX_PAYLOAD);
    int result = mock_has_valid();
    free(big_data);
    if (result == 1) { PASS(); } else { FAIL("should return 1 with max payload"); }
}

/* Test 11: 超限 payload → HasValid 返回 0 */
static void test_has_valid_oversize(void)
{
    TEST("HasValid returns 0 when length > MAX_PAYLOAD");
    mock_flash_reset();
    uint32_t *header = (uint32_t *)mock_flash;
    header[0] = APP_SCRIPT_FLASH_MAGIC;
    header[1] = APP_SCRIPT_FLASH_VERSION;
    header[2] = APP_SCRIPT_FLASH_MAX_PAYLOAD + 1U;  /* 超限 */
    header[3] = 0U;
    if (mock_has_valid() == 0) { PASS(); } else { FAIL("should return 0 when length exceeds max"); }
}

/* Test 12: 非 4 字节对齐的脚本 → CRC32 位置正确 */
static void test_has_valid_unaligned_length(void)
{
    TEST("HasValid with unaligned script length (7 bytes)");
    const uint8_t data[] = {0x0F, 'p', 'y', 'o', 0x01, 0x02, 0x03};  /* 7 bytes */
    build_valid_script(data, sizeof(data));
    if (mock_has_valid() == 1) { PASS(); } else { FAIL("should return 1 with unaligned length"); }
}

/* Test 13: CRC32 写入位置与读取位置一致 (1-16 字节全部测试) */
static void test_crc_position_consistency(void)
{
    TEST("CRC32 position consistency (1-16 bytes)");
    int all_ok = 1;
    for (uint32_t len = 1U; len <= 16U; len++)
    {
        uint8_t data[16];
        for (uint32_t i = 0U; i < len; i++) data[i] = (uint8_t)(i + len);
        build_valid_script(data, len);
        if (mock_has_valid() != 1)
        {
            printf("\n    CRC32 position mismatch at length=%u", (unsigned)len);
            all_ok = 0;
            break;
        }
    }
    if (all_ok) { PASS(); } else { FAIL("CRC32 position mismatch for some lengths"); }
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("\n=== PikaPython Script Flash 单元测试 ===\n\n");

    test_crc32_empty();
    test_crc32_known_vector();
    test_crc32_chaining();
    test_has_valid_empty_flash();
    test_has_valid_good_script();
    test_has_valid_corrupt_crc();
    test_has_valid_bad_magic();
    test_has_valid_bad_version();
    test_has_valid_zero_length();
    test_has_valid_max_payload();
    test_has_valid_oversize();
    test_has_valid_unaligned_length();
    test_crc_position_consistency();

    printf("\n=== 结果: %d PASS, %d FAIL ===\n\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}