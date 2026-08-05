#ifndef DRV_IR_REFLECT_H
#define DRV_IR_REFLECT_H

#include <stdint.h>

void drv_ir_reflect_init(void);
void drv_ir_reflect_sample(void);
void drv_ir_reflect_sample_callback(void *arg);
uint16_t drv_ir_reflect_get_raw(void);

#endif
