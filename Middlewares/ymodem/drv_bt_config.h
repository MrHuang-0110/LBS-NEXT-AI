#ifndef DRV_BT_CONFIG_H
#define DRV_BT_CONFIG_H

#include <stdint.h>

#define DRV_BT_BAUD             115200U
#define DRV_BT_NAME             "LBS_NEXT_AI"
#define DRV_BT_AT_TIMEOUT_MS    500U
#define DRV_BT_DRAIN_MS         40U
#define DRV_BT_AT_GAP_MS        100U
#define DRV_BT_FACTORY_WAIT_MS  600U
#define DRV_BT_RST_WAIT_MS      300U

#define DRV_BT_TX_CHUNK_MAX     64U
#define DRV_BT_TX_GAP_MS        30U
#define DRV_BT_MONITOR_TX_MAX   248U
#define DRV_BT_YMODEM_IDLE_MS   15000U
#define DRV_BT_LED_BLINK_MS     400U

#endif
