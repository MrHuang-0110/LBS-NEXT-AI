/**
 * test_frame_parser.c — 协议帧解析 单元测试（host 模拟）
 *
 * 编译: gcc -Wall -Wextra -std=c99 -o test_frame_parser test_frame_parser.c && ./test_frame_parser
 *
 * 背景: 0xB6/0xB9（脚本启停）、0xBE/0xBA（监控开关）均为无数据载荷帧，
 *       帧格式 `0x5A|sID|oID|len|idx|data[len]|crc|0xA5`，len=0 时总长仅 7 字节。
 *       生产代码 dataAgreeAnalys()（src/protocol/frame.c）的 `length < 8` 检查
 *       会拒绝这类 7 字节帧 → USB/蓝牙两条通道的脚本启停指令全部被丢弃。
 *       修复：MIN_FRAME_SIZE 8→7，长度检查前移（先查长度再解引用 data[length-1]）。
 *
 * 本测试复制生产代码（src/protocol/frame.c 的 calculate_checksum 与
 * dataAgreeAnalys）到 host，验证 7 字节无数据帧可被解析、CRC/长度校验仍有效。
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* 复制生产代码常量与结构（src/protocol/frame.h）                      */
/* ------------------------------------------------------------------ */

#define AGREE_MEN_ERROR 0
#define AGREE_MEN_OK    1

#define FRAME_HEADER    0x5A
#define SRC_ID          0x97
#define DEST_ID         0x98
#define FRAME_FOOTER    0xA5
#define MAX_FRAME_SIZE  300
#define MIN_FRAME_SIZE  7    /* 修复：头+源+目标+长度+类型+校验+尾 = 7 字节（len=0 帧） */

typedef struct
{
  uint8_t  Head;
  uint8_t  sID;
  uint8_t  oID;
  uint16_t length;
  uint8_t  index;
  uint8_t  data[256];
  uint8_t  crc;
  uint8_t  tard;
} _AGREEMENT;

/* ------------------------------------------------------------------ */
/* 复制生产代码逻辑（src/protocol/frame.c，修复后版本）                */
/* ------------------------------------------------------------------ */

static uint8_t calculate_checksum(const uint8_t *data, size_t length)
{
    uint32_t checksum = 0;
    for (size_t i = 0; i < length; i++)
    {
        checksum += data[i];
    }
    return (uint8_t)(checksum & 0xFF);
}

uint8_t dataAgreeAnalys(_AGREEMENT *_agreement_, uint8_t *data, uint16_t length)
{
    /* 先查长度再解引用：data[length-1] 在 length 过小时会越界（修复前先查 data[0]） */
    if (length < MIN_FRAME_SIZE)
    {
        return AGREE_MEN_ERROR;
    }
    /* 防截断帧：声明的数据长度不得超出实际帧长。完整帧 = 7 + data[3]，故 data[3] <= length-7 */
    if (data[3] > (length - 7U))
    {
        return AGREE_MEN_ERROR;
    }
    if (data[0] != 0x5A || data[length - 1] != 0xA5)
    {
        return AGREE_MEN_ERROR;
    }

    uint8_t _mycrc_;
    _mycrc_ = calculate_checksum((const uint8_t *)data, (size_t)(length - 2));

    if (_mycrc_ != data[length - 2])
    {
        return AGREE_MEN_ERROR;
    }
    else
    {
        memset(_agreement_->data, 0, 256);
        _agreement_->Head = data[0];
        _agreement_->sID = data[1];
        _agreement_->oID = data[2];
        _agreement_->length = data[3];
        _agreement_->index = data[4];

        memcpy(_agreement_->data, data + 5, data[3]);

        _agreement_->crc = data[length - 2];
        _agreement_->tard = data[length - 1];
        return AGREE_MEN_OK;
    }
}

/* ------------------------------------------------------------------ */
/* 辅助: 构建帧                                                        */
/* ------------------------------------------------------------------ */

#define FRAME_MAX 300

static void build_frame(uint8_t *out, uint16_t *out_len,
                        uint8_t index, const uint8_t *payload, uint8_t payload_len)
{
    uint16_t pos = 0;
    out[pos++] = FRAME_HEADER;
    out[pos++] = SRC_ID;
    out[pos++] = DEST_ID;
    out[pos++] = payload_len;
    out[pos++] = index;
    if (payload != NULL && payload_len > 0U)
    {
        memcpy(out + pos, payload, payload_len);
        pos += payload_len;
    }
    uint8_t crc = calculate_checksum(out, (size_t)pos);
    out[pos++] = crc;
    out[pos++] = FRAME_FOOTER;
    *out_len = pos;
}

/* ------------------------------------------------------------------ */
/* 测试框架                                                           */
/* ------------------------------------------------------------------ */

static int g_pass = 0;
static int g_fail = 0;

#define TEST(name) do { \
    printf("  %-58s ", name); \
} while (0)

#define PASS() do { \
    printf("PASS\n"); g_pass++; \
} while (0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); g_fail++; \
} while (0)

/* ------------------------------------------------------------------ */
/* 测试用例                                                           */
/* ------------------------------------------------------------------ */

/* Test 1: 0xB6 无数据帧（7 字节）可解析 —— 复现 bug 场景 */
static void test_b6_no_payload_frame(void)
{
    TEST("0xB6 no-payload frame (7 bytes) parses OK");
    uint8_t buf[FRAME_MAX];
    uint16_t len;
    build_frame(buf, &len, 0xB6, NULL, 0U);
    if (len != 7U)
    {
        FAIL("expected 7-byte frame");
        return;
    }
    _AGREEMENT frame;
    uint8_t r = dataAgreeAnalys(&frame, buf, len);
    if (r != AGREE_MEN_OK)
    {
        FAIL("7-byte 0xB6 frame rejected (length<8 bug)");
        return;
    }
    if (frame.index != 0xB6)
    {
        FAIL("index mismatch");
        return;
    }
    PASS();
}

/* Test 2: 0xB9 无数据帧同样可解析 */
static void test_b9_no_payload_frame(void)
{
    TEST("0xB9 no-payload frame (7 bytes) parses OK");
    uint8_t buf[FRAME_MAX];
    uint16_t len;
    build_frame(buf, &len, 0xB9, NULL, 0U);
    _AGREEMENT frame;
    uint8_t r = dataAgreeAnalys(&frame, buf, len);
    if (r != AGREE_MEN_OK || frame.index != 0xB9)
    {
        FAIL("7-byte 0xB9 frame rejected");
        return;
    }
    PASS();
}

/* Test 3: 带 1 字节载荷的帧（8 字节）仍可解析 */
static void test_payload_frame(void)
{
    TEST("frame with 1-byte payload (8 bytes) parses OK");
    uint8_t buf[FRAME_MAX];
    uint16_t len;
    uint8_t payload[] = {0x42};
    build_frame(buf, &len, 0xC1, payload, sizeof(payload));
    _AGREEMENT frame;
    uint8_t r = dataAgreeAnalys(&frame, buf, len);
    if (r != AGREE_MEN_OK)
    {
        FAIL("8-byte payload frame rejected");
        return;
    }
    if (frame.length != 1U || frame.data[0] != 0x42)
    {
        FAIL("payload mismatch");
        return;
    }
    PASS();
}

/* Test 4: 大载荷帧（len=200）仍可解析且 CRC 正确 */
static void test_large_payload_frame(void)
{
    TEST("frame with 200-byte payload parses OK");
    uint8_t buf[FRAME_MAX];
    uint16_t len;
    uint8_t payload[200];
    for (uint16_t i = 0U; i < sizeof(payload); i++)
    {
        payload[i] = (uint8_t)(i & 0xFFU);
    }
    build_frame(buf, &len, 0xAA, payload, sizeof(payload));
    _AGREEMENT frame;
    uint8_t r = dataAgreeAnalys(&frame, buf, len);
    if (r != AGREE_MEN_OK)
    {
        FAIL("200-byte payload frame rejected");
        return;
    }
    if (frame.length != 200U || memcmp(frame.data, payload, sizeof(payload)) != 0)
    {
        FAIL("payload mismatch");
        return;
    }
    PASS();
}

/* Test 5: CRC 损坏 → 拒绝 */
static void test_corrupt_crc(void)
{
    TEST("corrupt CRC rejected");
    uint8_t buf[FRAME_MAX];
    uint16_t len;
    build_frame(buf, &len, 0xB6, NULL, 0U);
    buf[len - 2] ^= 0xFFU;   /* 破坏 CRC 字节 */
    _AGREEMENT frame;
    uint8_t r = dataAgreeAnalys(&frame, buf, len);
    if (r != AGREE_MEN_ERROR)
    {
        FAIL("corrupt CRC accepted");
        return;
    }
    PASS();
}

/* Test 6: 帧尾错误（非 0xA5）→ 拒绝 */
static void test_bad_footer(void)
{
    TEST("bad footer (not 0xA5) rejected");
    uint8_t buf[FRAME_MAX];
    uint16_t len;
    build_frame(buf, &len, 0xB6, NULL, 0U);
    buf[len - 1] = 0x00;
    _AGREEMENT frame;
    uint8_t r = dataAgreeAnalys(&frame, buf, len);
    if (r != AGREE_MEN_ERROR)
    {
        FAIL("bad footer accepted");
        return;
    }
    PASS();
}

/* Test 7: 帧头错误（非 0x5A）→ 拒绝 */
static void test_bad_header(void)
{
    TEST("bad header (not 0x5A) rejected");
    uint8_t buf[FRAME_MAX];
    uint16_t len;
    build_frame(buf, &len, 0xB6, NULL, 0U);
    buf[0] = 0x00;
    _AGREEMENT frame;
    uint8_t r = dataAgreeAnalys(&frame, buf, len);
    if (r != AGREE_MEN_ERROR)
    {
        FAIL("bad header accepted");
        return;
    }
    PASS();
}

/* Test 8: 长度不足 7 字节 → 拒绝（且不越界读 data[length-1]） */
static void test_too_short_frame(void)
{
    TEST("frame shorter than 7 bytes rejected (no OOB read)");
    uint8_t buf[6] = {0x5A, 0x97, 0x98, 0x00, 0xB6, 0x5A};
    _AGREEMENT frame;
    uint8_t r = dataAgreeAnalys(&frame, buf, 6U);
    if (r != AGREE_MEN_ERROR)
    {
        FAIL("6-byte frame accepted");
        return;
    }
    PASS();
}

/* Test 9: 截断帧（声明的数据长度超出实际帧长）→ 拒绝，不把帧外字节当数据。
 * 帧总长 = 7 + data[3]；length=8 时合法 data[3] 上限为 1，声明 2 即为截断 */
static void test_truncated_frame(void)
{
    TEST("truncated frame (declared len > actual) rejected");
    uint8_t buf[8] = {0x5A, 0x97, 0x98, 0x02, 0xB6, 0x01, 0x02, 0xA5};
    _AGREEMENT frame;
    uint8_t r = dataAgreeAnalys(&frame, buf, 8U);
    if (r != AGREE_MEN_ERROR)
    {
        FAIL("truncated frame accepted (declared len=2 but 8 bytes only fits len=1)");
        return;
    }
    PASS();
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("\n=== 协议帧解析 单元测试 ===\n\n");

    test_b6_no_payload_frame();
    test_b9_no_payload_frame();
    test_payload_frame();
    test_large_payload_frame();
    test_corrupt_crc();
    test_bad_footer();
    test_bad_header();
    test_too_short_frame();
    test_truncated_frame();

    printf("\n=== 结果: %d PASS, %d FAIL ===\n\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
