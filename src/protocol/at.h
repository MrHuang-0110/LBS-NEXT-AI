#ifndef __AT_H
#define __AT_H
#include <stdint.h>
void At_Init(void);
void At_SyncFromModule(void);
const char *At_GetName(void);
const char *At_GetAdvData(void);
void At_SetName(const char *name);
void At_SetAdvData(const char *hex);
void At_SendLine(const char *line);
uint8_t At_IsBusy(void);
/* 统一 AT 通道（Task 17，吸收 blue_at_cmd 的重试/OK 判定语义）。
 * line 不含结尾 \r\n；成功/失败由 *out_ok 表达（1=OK，0=ERROR/超时）。
 * 返回值：1=已得到模块明确结论（OK 或 ERROR），0=3 次重试均超时/发送失败。 */
uint8_t At_Exchange(const char *line, uint8_t *out_ok);
#endif
