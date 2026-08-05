#include "drv_ymodem.h"
#include "drv_mem.h"
#include "drv_bt_config.h"
#include "delay.h"
#include "stm32f1xx_hal.h"
#include <string.h>

#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18
#define CRC_C 0x43

#define BLK128 128U
#define BLK1K  1024U
#define YMODEM_POLL_MS      10U
#define YMODEM_TIMEOUT_MS   5000U
#define YMODEM_SEND_C_MS    1000U
#define YMODEM_IDLE_USB_MS  3000U
#define YMODEM_INTER_USB_MS 200U
#define YMODEM_INTER_BT_MS  800U
#define YMODEM_BT_CRC_C_MS  400U

static uint8_t s_pkt[1035];
static uint32_t s_bt_last_crc_c_ms;
static uint8_t s_rx_push;
static uint8_t s_rx_push_valid;
static uint32_t s_last_rx_ms;
static uint32_t s_idle_limit_ms;

static void ymodem_touch_rx(void)
{
    s_last_rx_ms = HAL_GetTick();
}

static int ymodem_idle_expired(void)
{
    return ((HAL_GetTick() - s_last_rx_ms) > s_idle_limit_ms) ? 1 : 0;
}

static void ymodem_send_byte(DrvComm_t *comm, uint8_t b)
{
    comm->send_byte(b);
}

static void ymodem_link_flush(DrvComm_t *comm)
{
    if ((comm != NULL) && (comm->send_flush != NULL))
    {
        comm->send_flush();
    }
}

static int ymodem_abort(DrvComm_t *comm)
{
    ymodem_send_byte(comm, CAN);
    ymodem_send_byte(comm, CAN);
    ymodem_link_flush(comm);
    return -1;
}

static void ymodem_send_cstr(DrvComm_t *comm, const uint8_t *msg)
{
    uint16_t i = 0U;
    if ((comm == NULL) || (msg == NULL))
    {
        return;
    }
    while (msg[i] != 0U)
    {
        ymodem_send_byte(comm, msg[i]);
        i++;
    }
    ymodem_link_flush(comm);
}

static int ymodem_finish_ok(DrvComm_t *comm, uint32_t written)
{
    static const uint8_t ok_msg[] = "\r\nYMODEM OK\r\n";
    DrvMem_SetFileLength(written);
    ymodem_send_cstr(comm, ok_msg);
    return 0;
}

static uint16_t crc16_xmodem(const uint8_t *p, uint16_t n)
{
    uint16_t crc = 0U;
    uint16_t i;
    int k;
    for (i = 0U; i < n; i++)
    {
        uint32_t c = ((uint32_t)crc ^ ((uint32_t)p[i] << 8)) & 0xFFFFU;
        for (k = 0; k < 8; k++)
        {
            c = (c & 0x8000U) ? (((c << 1) ^ 0x1021U) & 0xFFFFU) : ((c << 1) & 0xFFFFU);
        }
        crc = (uint16_t)c;
    }
    return crc;
}

static int read_byte_pushed(DrvComm_t *comm, uint8_t *out, uint32_t timeout_ms)
{
    if (s_rx_push_valid)
    {
        *out = s_rx_push;
        s_rx_push_valid = 0U;
        return 1;
    }
    return comm->read_byte(out, timeout_ms);
}

static int pkt_type_allowed(DrvComm_t *comm, uint8_t hdr)
{
    if (hdr == SOH)
    {
        return 1;
    }
    if (hdr == STX)
    {
        if (comm != NULL && comm->ymodem_soh_only != 0U)
        {
            return 0;
        }
        return 1;
    }
    return 0;
}

static uint32_t ymodem_inter_byte_ms(DrvComm_t *comm)
{
    return (comm != NULL && comm->ymodem_soh_only != 0U) ? (uint32_t)YMODEM_INTER_BT_MS
                                                         : (uint32_t)YMODEM_INTER_USB_MS;
}

static void ymodem_maybe_send_crc_c(DrvComm_t *comm, uint8_t header_done, uint32_t written, uint32_t file_len)
{
    if ((comm == NULL) || (comm->ymodem_soh_only == 0U) || (header_done == 0U) ||
        (written >= file_len))
    {
        return;
    }
    if ((HAL_GetTick() - s_bt_last_crc_c_ms) >= YMODEM_BT_CRC_C_MS)
    {
        ymodem_send_byte(comm, CRC_C);
        ymodem_link_flush(comm);
        s_bt_last_crc_c_ms = HAL_GetTick();
    }
}

static int read_block(DrvComm_t *comm, uint8_t *pkt, uint32_t first_to, uint32_t inter_to)
{
    uint16_t dl;
    uint16_t more;
    uint16_t j;
    if (!read_byte_pushed(comm, &pkt[0], first_to))
    {
        return -1;
    }
    ymodem_touch_rx();
    if (pkt[0] == EOT)
    {
        return 1;
    }
    if (pkt[0] == CAN)
    {
        return -2;
    }
    if (pkt_type_allowed(comm, pkt[0]) == 0)
    {
        return -1;
    }
    dl = (pkt[0] == SOH) ? BLK128 : BLK1K;
    more = (uint16_t)(4U + dl);
    for (j = 0U; j < more; j++)
    {
        if (!read_byte_pushed(comm, &pkt[1U + j], inter_to))
        {
            return -1;
        }
        ymodem_touch_rx();
    }
    return 0;
}

static uint32_t parse_file_size(const uint8_t *p, uint16_t n)
{
    uint32_t i = 0U;
    uint32_t sz = 0U;
    uint32_t j;
    while (i < n && p[i] != 0U)
    {
        i++;
    }
    if (i >= n)
    {
        return 0U;
    }
    i++;
    while (i < n && (p[i] == (uint8_t)' ' || p[i] == 0U))
    {
        i++;
    }
    j = i;
    while (j < n && p[j] >= '0' && p[j] <= '9')
    {
        sz = sz * 10U + (uint32_t)(p[j] - (uint8_t)'0');
        j++;
    }
    return sz;
}

int DrvYmodem_ReceiveToRam(DrvComm_t *comm)
{
    uint8_t *const pkt = s_pkt;
    uint8_t *const ram = DrvMem_GetFileBuffer();
    const uint32_t ram_max = DrvMem_GetFileBufferSize();
    uint32_t ram_off = 0U;
    uint32_t file_len = 0U;
    uint32_t written = 0U;
    uint8_t blk_expect = 1U;
    uint8_t eot_count = 0U;
    uint8_t header_done = 0U;
    uint8_t expect_end_soh = 0U;
    int rr;
    int first_pkt = 1;
    uint32_t t0;
    uint32_t t1;

    if (comm == NULL || comm->send_byte == NULL || comm->read_byte == NULL)
    {
        return -1;
    }

    DrvMem_ClearFile();
    comm->flush_rx();
    s_rx_push_valid = 0U;
    s_bt_last_crc_c_ms = 0U;
    s_idle_limit_ms = (comm->ymodem_soh_only != 0U) ? (uint32_t)DRV_BT_YMODEM_IDLE_MS
                                                      : (uint32_t)YMODEM_IDLE_USB_MS;
    ymodem_touch_rx();
    if (comm->send_flush != NULL)
    {
        comm->send_flush();
    }

    t0 = HAL_GetTick();
    t1 = HAL_GetTick();
    /* 进入 YMODEM 后立即发 'C'，勿等满 YMODEM_SEND_C_MS（否则 PC 端短超时易失败） */
    ymodem_send_byte(comm, CRC_C);
    ymodem_link_flush(comm);
    t1 = HAL_GetTick();
    for (;;)
    {
        if ((HAL_GetTick() - t0) > YMODEM_TIMEOUT_MS)
        {
            return -1;
        }
        if ((HAL_GetTick() - t1) > YMODEM_SEND_C_MS)
        {
            ymodem_send_byte(comm, CRC_C);
            ymodem_link_flush(comm);
            t1 = HAL_GetTick();
        }
        ymodem_link_flush(comm);
        rr = read_block(comm, pkt, YMODEM_POLL_MS, ymodem_inter_byte_ms(comm));
        if (rr == 0)
        {
            break;
        }
    }

    for (;;)
    {
        uint32_t inter_to = ymodem_inter_byte_ms(comm);
        uint16_t data_len;
        uint16_t crc_calc;
        uint16_t crc_be;
        uint8_t bn;
        uint8_t bnx;
        uint32_t chunk;

        if (!first_pkt)
        {
            if (ymodem_idle_expired() != 0)
            {
                return ymodem_abort(comm);
            }
            rr = read_block(comm, pkt, YMODEM_POLL_MS, inter_to);
            if (rr == -1)
            {
                ymodem_maybe_send_crc_c(comm, header_done, written, file_len);
                if (ymodem_idle_expired() != 0)
                {
                    return ymodem_abort(comm);
                }
                continue;
            }
            if (rr == 1)
            {
                if (eot_count == 0U)
                {
                    ymodem_send_byte(comm, NAK);
                    ymodem_link_flush(comm);
                    eot_count = 1U;
                    continue;
                }
                ymodem_send_byte(comm, ACK);
                ymodem_send_byte(comm, CRC_C);
                ymodem_link_flush(comm);
                ymodem_touch_rx();
                if (ymodem_idle_expired() != 0)
                {
                    return ymodem_abort(comm);
                }
                rr = read_block(comm, pkt, YMODEM_POLL_MS, inter_to);
                if (rr == -1)
                {
                    ymodem_maybe_send_crc_c(comm, header_done, written, file_len);
                    if (ymodem_idle_expired() != 0)
                    {
                        return ymodem_abort(comm);
                    }
                    continue;
                }
                if (rr != 0)
                {
                    if (file_len > 0U && written >= file_len)
                    {
                        ymodem_send_byte(comm, ACK);
                        ymodem_link_flush(comm);
                        return ymodem_finish_ok(comm, written);
                    }
                    return -1;
                }
                expect_end_soh = 1U;
            }
            if (rr != 0)
            {
                return ymodem_abort(comm);
            }
        }
        else
        {
            first_pkt = 0;
        }

        ymodem_touch_rx();
        data_len = (pkt[0] == SOH) ? BLK128 : BLK1K;
        bn = pkt[1];
        bnx = pkt[2];
        if ((uint8_t)(bn + bnx) != 0xFFU)
        {
            ymodem_send_byte(comm, NAK);
            ymodem_link_flush(comm);
            continue;
        }

        crc_be = (uint16_t)(((uint16_t)pkt[3U + data_len] << 8) | (uint16_t)pkt[3U + data_len + 1U]);
        crc_calc = crc16_xmodem(&pkt[3], data_len);
        if (crc_calc != crc_be)
        {
            ymodem_send_byte(comm, NAK);
            ymodem_link_flush(comm);
            continue;
        }

        if (bn == 0U && header_done == 0U)
        {
            file_len = parse_file_size(&pkt[3], data_len);
            if (file_len == 0U || file_len > ram_max)
            {
                ymodem_send_byte(comm, CAN);
                ymodem_link_flush(comm);
                return -1;
            }
            ymodem_send_byte(comm, ACK);
            ymodem_send_byte(comm, CRC_C);
            ymodem_link_flush(comm);
            s_bt_last_crc_c_ms = HAL_GetTick();
            blk_expect = 1U;
            header_done = 1U;
            continue;
        }

        if (expect_end_soh != 0U)
        {
            ymodem_send_byte(comm, ACK);
            ymodem_link_flush(comm);
            return ymodem_finish_ok(comm, written);
        }

        if ((bn == 0U) && (header_done != 0U))
        {
            ymodem_send_byte(comm, ACK);
            ymodem_link_flush(comm);
            return ymodem_finish_ok(comm, written);
        }

        if (bn != blk_expect)
        {
            ymodem_send_byte(comm, (bn == (uint8_t)(blk_expect - 1U)) ? ACK : NAK);
            ymodem_link_flush(comm);
            continue;
        }

        chunk = data_len;
        if (written + chunk > file_len)
        {
            chunk = file_len - written;
        }
        if (chunk > 0U)
        {
            if (ram_off + chunk > ram_max)
            {
                ymodem_send_byte(comm, CAN);
                ymodem_link_flush(comm);
                return -1;
            }
            memcpy(&ram[ram_off], &pkt[3], chunk);
            ram_off += chunk;
            written += chunk;
        }

        ymodem_send_byte(comm, ACK);
        ymodem_link_flush(comm);
        blk_expect++;
        if (blk_expect == 0U)
        {
            blk_expect = 1U;
        }
    }
}
