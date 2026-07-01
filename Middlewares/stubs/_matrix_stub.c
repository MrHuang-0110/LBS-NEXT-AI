#include "PikaObj.h"
#include "_matrix.h"
#include "matrix_port_stub.h"

void _matrix_clear(PikaObj *self)
{
    (void)self;
    matrix_port_clear();
}

void _matrix_set_brightness(PikaObj *self, pika_float brigntness)
{
    (void)self;
    matrix_port_set_brightness((uint8_t)brigntness);
}

void _matrix_set_pixel(PikaObj *self, pika_float x, pika_float y)
{
    (void)self;
    matrix_port_set_pixel((uint8_t)x, (uint8_t)y);
}

void _matrix_set_pixel_brightness(PikaObj *self, pika_float x, pika_float y, pika_float brigntness)
{
    (void)self;
    matrix_port_set_pixel_brightness((uint8_t)x, (uint8_t)y, (uint8_t)brigntness);
}

void _matrix_show(PikaObj *self, pika_float bufer1, pika_float bufer2, pika_float bufer3,
                  pika_float bufer4, pika_float bufer5, pika_float bufer6, pika_float bufer7)
{
    uint8_t pattern[7];
    (void)self;
    pattern[0] = (uint8_t)bufer1;
    pattern[1] = (uint8_t)bufer2;
    pattern[2] = (uint8_t)bufer3;
    pattern[3] = (uint8_t)bufer4;
    pattern[4] = (uint8_t)bufer5;
    pattern[5] = (uint8_t)bufer6;
    pattern[6] = (uint8_t)bufer7;
    matrix_port_display_pattern(pattern);
}

void _matrix_show_roll(PikaObj *self, char *text)
{
    (void)self;
    matrix_port_scroll_text(text, NULL);
}
