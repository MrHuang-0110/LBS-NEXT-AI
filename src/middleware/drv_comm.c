#include "drv_comm.h"
#include "drv_bt_config.h"
#include "usbd_cdc_interface.h"
#include "delay.h"
#include "stm32f1xx_hal.h"
#include <string.h>

#define USB_RING_SIZE   2048U
#define BT_RING_SIZE    2048U

static uint8_t s_usb_ring[USB_RING_SIZE];
static volatile uint16_t s_usb_head;
static volatile uint16_t s_usb_tail;

static uint8_t s_bt_ring[BT_RING_SIZE];
static volatile uint16_t s_bt_head;
static volatile uint16_t s_bt_tail;
static volatile uint8_t s_bt_passthrough;

static uint8_t s_bt_tx_buf[DRV_BT_TX_CHUNK_MAX];
static uint8_t s_bt_tx_cnt;

#define USB_TX_BUF_SIZE   16U
static uint8_t s_usb_tx_buf[USB_TX_BUF_SIZE];
static uint8_t s_usb_tx_cnt;

extern UART_HandleTypeDef *getusartHandle(uint8_t num);
extern uint8_t uart_transmit_it(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len);

static uint16_t ring_count(volatile uint16_t head, volatile uint16_t tail, uint16_t cap)
{
    return (head >= tail) ? (uint16_t)(head - tail) : (uint16_t)(cap - tail + head);
}

static void ring_push_byte(uint8_t *ring, volatile uint16_t *head, volatile uint16_t *tail,
                           uint16_t cap, uint8_t b)
{
    uint16_t next = (uint16_t)((*head + 1U) % cap);
    if (next != *tail)
    {
        ring[*head] = b;
        *head = next;
    }
}

static int ring_pop_byte(uint8_t *ring, volatile uint16_t *head, volatile uint16_t *tail,
                         uint16_t cap, uint8_t *out)
{
    if (*head == *tail)
    {
        return 0;
    }
    *out = ring[*tail];
    *tail = (uint16_t)((*tail + 1U) % cap);
    return 1;
}

void DrvUsbRing_Init(void)
{
    s_usb_head = 0U;
    s_usb_tail = 0U;
    s_usb_tx_cnt = 0U;
}

void DrvUsbRing_Push(const uint8_t *data, uint32_t len)
{
    uint32_t i;
    if (data == NULL)
    {
        return;
    }
    for (i = 0U; i < len; i++)
    {
        ring_push_byte(s_usb_ring, &s_usb_head, &s_usb_tail, USB_RING_SIZE, data[i]);
    }
}

int DrvUsbRing_ReadByte(uint8_t *out, uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    if (out == NULL)
    {
        return 0;
    }
    for (;;)
    {
        if (ring_pop_byte(s_usb_ring, &s_usb_head, &s_usb_tail, USB_RING_SIZE, out))
        {
            return 1;
        }
        if ((HAL_GetTick() - t0) >= timeout_ms)
        {
            return 0;
        }
        delay_ms(1);
    }
}

void DrvUsbRing_Flush(void)
{
    s_usb_tail = s_usb_head;
}

void DrvBtRing_Init(void)
{
    s_bt_head = 0U;
    s_bt_tail = 0U;
    s_bt_passthrough = 0U;
}

void DrvBtRing_SetPassthrough(uint8_t on)
{
    s_bt_passthrough = (on != 0U) ? 1U : 0U;
}

void DrvBtRing_Push(const uint8_t *data, uint32_t len)
{
    uint32_t i;
    if (data == NULL)
    {
        return;
    }
    for (i = 0U; i < len; i++)
    {
        ring_push_byte(s_bt_ring, &s_bt_head, &s_bt_tail, BT_RING_SIZE, data[i]);
    }
}

int DrvBtRing_ReadByte(uint8_t *out, uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    if (out == NULL)
    {
        return 0;
    }
    for (;;)
    {
        if (ring_pop_byte(s_bt_ring, &s_bt_head, &s_bt_tail, BT_RING_SIZE, out))
        {
            return 1;
        }
        if ((HAL_GetTick() - t0) >= timeout_ms)
        {
            return 0;
        }
        delay_ms(1);
    }
}

void DrvBtRing_Flush(void)
{
    s_bt_tail = s_bt_head;
}

static void comm_usb_flush_tx(void)
{
    if (s_usb_tx_cnt == 0U)
    {
        return;
    }
    cdc_vcp_data_tx(s_usb_tx_buf, s_usb_tx_cnt);
    s_usb_tx_cnt = 0U;
}

static void comm_usb_send(uint8_t b)
{
    if (s_usb_tx_cnt >= USB_TX_BUF_SIZE)
    {
        comm_usb_flush_tx();
    }
    s_usb_tx_buf[s_usb_tx_cnt++] = b;
}

static int comm_usb_read(uint8_t *out, uint32_t ms)
{
    return DrvUsbRing_ReadByte(out, ms);
}

static void comm_usb_flush_rx(void)
{
    DrvUsbRing_Flush();
}

static void comm_bt_tx_flush(void)
{
    UART_HandleTypeDef *huart = getusartHandle(5);
    if ((s_bt_tx_cnt > 0U) && (huart != NULL))
    {
        uart_transmit_it(huart, s_bt_tx_buf, s_bt_tx_cnt);
        s_bt_tx_cnt = 0U;
        delay_ms(DRV_BT_TX_GAP_MS);
    }
}

static void comm_bt_send(uint8_t b)
{
    s_bt_tx_buf[s_bt_tx_cnt++] = b;
    if (s_bt_tx_cnt >= DRV_BT_TX_CHUNK_MAX)
    {
        comm_bt_tx_flush();
    }
}

static int comm_bt_read(uint8_t *out, uint32_t ms)
{
    return DrvBtRing_ReadByte(out, ms);
}

static void comm_bt_flush_rx(void)
{
    comm_bt_tx_flush();
    DrvBtRing_Flush();
}

static DrvComm_t s_usb_comm =
{
    DRV_COMM_USB, comm_usb_send, comm_usb_flush_tx, comm_usb_read, comm_usb_flush_rx, 0U
};

static DrvComm_t s_bt_comm =
{
    DRV_COMM_UART5, comm_bt_send, comm_bt_tx_flush, comm_bt_read, comm_bt_flush_rx, 1U
};

int DrvComm_Bind(DrvComm_t *comm, DrvCommPort_t port)
{
    if (comm == NULL)
    {
        return -1;
    }
    if (port == DRV_COMM_USB)
    {
        *comm = s_usb_comm;
        return 0;
    }
    if (port == DRV_COMM_UART5)
    {
        *comm = s_bt_comm;
        return 0;
    }
    return -1;
}

const char *DrvComm_PortName(DrvCommPort_t port)
{
    switch (port)
    {
        case DRV_COMM_USB: return "USB";
        case DRV_COMM_UART5: return "BT";
        default: return "?";
    }
}

uint8_t DrvComm_BtSendMonitor(const char *str)
{
    UART_HandleTypeDef *huart = getusartHandle(5);
    static uint8_t tx_buf[DRV_BT_MONITOR_TX_MAX];
    uint16_t len;
    uint16_t max_payload;

    if ((str == NULL) || (huart == NULL))
    {
        return 0U;
    }
    if (HAL_UART_GetState(huart) != HAL_UART_STATE_READY)
    {
        return 0U;
    }

    max_payload = (uint16_t)(DRV_BT_MONITOR_TX_MAX - 2U);
    len = (uint16_t)strlen(str);
    if (len > max_payload)
    {
        len = max_payload;
    }

    memcpy(tx_buf, str, len);
    tx_buf[len++] = '\r';
    tx_buf[len++] = '\n';
    return uart_transmit_it(huart, tx_buf, len);
}

uint8_t DrvComm_BtSendData(const uint8_t *data, uint16_t len)
{
    extern UART_HandleTypeDef *getusartHandle(uint8_t num);
    extern uint8_t uart_transmit_it(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len);
    if (data == NULL || len == 0U) { return 0U; }
    return uart_transmit_it(getusartHandle(5), (uint8_t *)data, len);
}
