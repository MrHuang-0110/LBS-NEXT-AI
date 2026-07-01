#include "display.h"
#include "ui_manager.h"
#include "matrix_port.h"
#include "_matrix.h"


void _matrix_clear(PikaObj *self)
{ 
  matrix_port_clear();
}
void _matrix_set_brightness(PikaObj *self, pika_float brigntness)
{ 
  matrix_port_set_brightness(brigntness);
}
void _matrix_set_pixel(PikaObj *self, pika_float x, pika_float y)
{ 
   matrix_port_set_pixel(x,y);
}
void _matrix_set_pixel_brightness(PikaObj *self, pika_float x, pika_float y, pika_float brigntness)
{ 
  matrix_port_set_pixel_brightness(x,y,brigntness);
}
void _matrix_show(PikaObj *self, pika_float bufer1, pika_float bufer2, pika_float bufer3, pika_float bufer4, pika_float bufer5, pika_float bufer6, pika_float bufer7)
{ 
	uint8_t temp_lamp[8];
	
  memset(temp_lamp,0,sizeof(temp_lamp));
	
   temp_lamp[0] = (unsigned char)bufer1;
	 temp_lamp[1] = (unsigned char)bufer2;
	 temp_lamp[2] = (unsigned char)bufer3;
	 temp_lamp[3]=  (unsigned char)bufer4;
	 temp_lamp[4]=  (unsigned char)bufer5;
	 temp_lamp[5] = (unsigned char)bufer6;
	 temp_lamp[6] = (unsigned char)bufer7;
	
   matrix_port_display_pattern(temp_lamp);
}
void _matrix_show_roll(PikaObj *self, char* text)
{ 
  matrix_port_scroll_text(text,uvm_exit);
}