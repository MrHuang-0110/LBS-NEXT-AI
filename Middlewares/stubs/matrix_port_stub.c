#include "matrix_port_stub.h"
#include "PikaVM.h"

bool uvm_exit(void)
{
    return (VMSignal_getCtrl() == VM_SIGNAL_CTRL_EXIT);
}

void matrix_port_init(void) {}
void matrix_port_display_pattern(const uint8_t pattern[7]) { (void)pattern; }
void matrix_port_scroll_text(const char *str, matrix_port_exit_check_t exit_check)
{
    (void)str;
    (void)exit_check;
}
void matrix_port_set_brightness(uint8_t brightness) { (void)brightness; }
void matrix_port_set_pixel(uint8_t x, uint8_t y) { (void)x; (void)y; }
void matrix_port_set_pixel_brightness(uint8_t x, uint8_t y, uint8_t b)
{
    (void)x; (void)y; (void)b;
}
void matrix_port_clear(void) {}
