#include "frame.h"
#include "malloc.h"

 
static uint8_t calculate_checksum(const uint8_t *data, size_t length) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum += data[i];
    }
    return checksum&0xFF;
}

uint8_t dataAgreeAnalys(_AGREEMENT *_agreement_,uint8_t *data,uint16_t length)
{ 
  if(data[0] != 0x5A || data[length - 1] != 0xA5)
   	  return AGREE_MEN_ERROR;

   if(length <8)
		  return AGREE_MEN_ERROR;

	 static uint8_t _mycrc_;
   _mycrc_ = calculate_checksum((const uint8_t *)data,length - 2);

   if(_mycrc_!=data[length - 2])
	 {
	    return AGREE_MEN_ERROR;
	 }
   else
   {
		 memset(_agreement_->data,0,256);
     _agreement_->Head = data[0];
	   _agreement_->sID = data[1];
	   _agreement_->oID = data[2];
	   _agreement_->length = data[3];
     _agreement_->index = data[4];

     memcpy(_agreement_->data,data+5,data[3]);

	   _agreement_->crc = data[length - 2];
	   _agreement_->tard = data[length - 1];
	  return AGREE_MEN_OK;
   }
}

void frame_parser_init(FrameParser *parser) {
	  
    parser->state = STATE_IDLE;
    parser->index = 0;
    parser->expected_length = 0;
    parser->calc_checksum = 0;
    parser->frame_valid = false;
	
    if(parser->buffer != NULL)
		{
		   myfree(SRAMIN,parser->buffer);
		}
		parser->buffer = mymalloc(SRAMIN,MAX_FRAME_SIZE);
		memset(parser->buffer,0,MAX_FRAME_SIZE);
}

bool frame_parser_process_byte(FrameParser *parser, uint8_t byte) {
    switch (parser->state) {
        case STATE_IDLE:
            if (byte == FRAME_HEADER) {
                parser->state = STATE_HEADER;
                parser->index = 0;
                parser->calc_checksum = 0;
                parser->buffer[parser->index++] = byte;
                parser->calc_checksum += byte;  // 初始化校验和
            }
            break;
            
        case STATE_HEADER:
            if (byte == SRC_ID) {
                parser->state = STATE_SRC_ID;
                parser->buffer[parser->index++] = byte;
                parser->calc_checksum += byte;
            } else {
                frame_parser_init(parser);
            }
            break;
            
        case STATE_SRC_ID:
            if (byte == DEST_ID) {
                parser->state = STATE_DEST_ID;
                parser->buffer[parser->index++] = byte;
                parser->calc_checksum += byte;
            } else {
                frame_parser_init(parser);
            }
            break;
            
        case STATE_DEST_ID:
            parser->state = STATE_LENGTH;
            parser->buffer[parser->index++] = byte;
            parser->calc_checksum += byte;
            parser->expected_length = byte;  // 长度字段
            break;
            
        case STATE_LENGTH:
            parser->state = STATE_TYPE;
            parser->buffer[parser->index++] = byte;
            parser->calc_checksum += byte;
            parser->frame_type = byte;  // 类型字段
            
            // 检查是否有数据字段
            if (parser->expected_length == 0) {
                parser->state = STATE_CHECKSUM;
            }
            break;
            
        case STATE_TYPE:
            // 这个状态应该只在expected_length>0时才会进入
            parser->state = STATE_DATA;
            parser->buffer[parser->index++] = byte;
            parser->calc_checksum += byte;
            parser->data_bytes_received = 1;  // 跟踪已接收的数据字节数
            
            // 检查是否已经接收完所有数据
            if (parser->data_bytes_received >= parser->expected_length) {
                parser->state = STATE_CHECKSUM;
            }
            break;
            
        case STATE_DATA:
            parser->buffer[parser->index++] = byte;
            parser->calc_checksum += byte;
            parser->data_bytes_received++;
            
            // 检查是否接收完所有数据
            if (parser->data_bytes_received >= parser->expected_length) {
                parser->state = STATE_CHECKSUM;
            }
            break;
            
        case STATE_CHECKSUM:
            parser->buffer[parser->index++] = byte;
            
            // 验证校验和
            if (byte == (parser->calc_checksum & 0xFF)) {
                parser->state = STATE_FOOTER;
            } else {
                frame_parser_init(parser);
            }
            break;
            
        case STATE_FOOTER:
            parser->buffer[parser->index++] = byte;
            if (byte == FRAME_FOOTER) {
                // 完整帧接收成功
                parser->frame_valid = true;
                return true;
            } else {
                frame_parser_init(parser);
            }
            break;
            
        default:
            frame_parser_init(parser);
            break;
    }
    
    // 检查缓冲区溢出
    if (parser->index >= MAX_FRAME_SIZE) {
        frame_parser_init(parser);
    }
    
    return false;
}
void MultiUart_SendFrame(void (*transerf_data)(void*,uint16_t),
															 uint8_t *data,
															 uint16_t len,
															 uint8_t index)
{
    if(transerf_data == NULL)return;
	
    _AGREEMENT frame;
    
    memset(frame.data,0,sizeof(frame.data));
	
    frame.length = len+7;
	
	  frame.data[0] = 0x5A;
	  frame.data[1] = 0x97;
	  frame.data[2] = 0x98;
	  frame.data[3] = len;
	  frame.data[4] = index;
	
	  for(uint16_t i = 0;i<len;i++)
	 { 
	   frame.data[5+i] = data[i];
	 }
	 
	 uint32_t checksum = 0;
	 for (size_t i = 0; i < frame.length - 2; i++) {
		checksum += frame.data[i];
   }	
	 
	 frame.data[len + 5] = checksum&0xFF;
	 frame.data[len + 6] = 0xA5;	
	
   transerf_data((void *)frame.data,frame.length);
}
