#ifndef DRV_COMM_H
#define DRV_COMM_H

#include <stdint.h>

typedef enum
{
    DRV_COMM_USB = 0,
    DRV_COMM_UART5,
    DRV_COMM_MAX
} DrvCommPort_t;

typedef struct
{
    DrvCommPort_t port;
    void (*send_byte)(uint8_t b);
    void (*send_flush)(void);
    int (*read_byte)(uint8_t *out, uint32_t timeout_ms);
    void (*flush_rx)(void);
    uint8_t ymodem_soh_only;
} DrvComm_t;

int DrvComm_Bind(DrvComm_t *comm, DrvCommPort_t port);
const char *DrvComm_PortName(DrvCommPort_t port);

void DrvUsbRing_Init(void);
void DrvUsbRing_Push(const uint8_t *data, uint32_t len);
int DrvUsbRing_ReadByte(uint8_t *out, uint32_t timeout_ms);
void DrvUsbRing_Flush(void);

void DrvBtRing_Init(void);
void DrvBtRing_Push(const uint8_t *data, uint32_t len);
int DrvBtRing_ReadByte(uint8_t *out, uint32_t timeout_ms);
void DrvBtRing_Flush(void);
void DrvBtRing_SetPassthrough(uint8_t on);

uint8_t DrvComm_BtSendMonitor(const char *str);

#endif
