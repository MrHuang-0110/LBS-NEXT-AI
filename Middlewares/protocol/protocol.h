#ifndef __PROTOCOL_H
#define __PROTOCOL_H
#include "sys.h"
#include "stdbool.h"
#include "stdlib.h"
#include "string.h"


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
}_AGREEMENT;

enum
{
  AGREE_MEN_ERROR,
  AGREE_MEN_OK,
};

#define FRAME_HEADER    0x5A
#define SRC_ID          0x97
#define DEST_ID         0x98
#define FRAME_FOOTER    0xA5
#define MAX_FRAME_SIZE  300  // 最大帧长度
#define MIN_FRAME_SIZE  8    // 最小帧长度 (头+源+目标+长度+类型+校验+尾)

typedef enum {
    STATE_IDLE,         // 空闲状态
    STATE_HEADER,       // 接收帧头
    STATE_SRC_ID,       // 接收源ID
    STATE_DEST_ID,      // 接收目标ID
    STATE_LENGTH,       // 接收长度
    STATE_TYPE,         // 接收类型
    STATE_DATA,         // 接收数据
    STATE_CHECKSUM,     // 接收校验和
    STATE_FOOTER        // 接收帧尾
} ParserState;

typedef struct {
    ParserState state;              // 当前解析状态
    uint8_t *buffer; // 帧缓冲区
    uint16_t index;                // 当前写入位置
    uint16_t expected_length;     // 预期数据长度
	  uint16_t data_bytes_received;
    uint8_t calc_checksum;       // 计算的校验和
    uint8_t frame_type;          // 帧类型
    bool frame_valid;            // 帧有效标志
} FrameParser;



void frame_parser_init(FrameParser *parser);
uint8_t dataAgreeAnalys(_AGREEMENT *_agreement_,uint8_t *data,uint16_t length);
bool frame_parser_process_byte(FrameParser *parser, uint8_t byte);
void MultiUart_SendFrame(void (*transerf_data)(void*,uint16_t),uint8_t *data,uint16_t len,uint8_t index);
#endif
