#ifndef MATRIX_PORT_STUB_H
#define MATRIX_PORT_STUB_H

#include <stdint.h>
#include <stdbool.h>

typedef bool (*matrix_port_exit_check_t)(void);

void matrix_port_init(void);
void matrix_port_display_pattern(const uint8_t pattern[7]);
void matrix_port_scroll_text(const char *str, matrix_port_exit_check_t exit_check);
void matrix_port_set_brightness(uint8_t brightness);
void matrix_port_set_pixel(uint8_t x, uint8_t y);
void matrix_port_set_pixel_brightness(uint8_t x, uint8_t y, uint8_t b);
void matrix_port_clear(void);

bool uvm_exit(void);

#endif
