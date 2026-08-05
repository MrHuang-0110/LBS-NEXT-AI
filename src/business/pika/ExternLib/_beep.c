#include "_beep.h"
#include "math.h"
#include "beep.h"


void _beep_play_muic(PikaObj *self, char* feq, pika_float ms)
{ 
  beep_play_piano_melody((const char*)feq,(uint16_t)fabs(ms*1000.0f));
}