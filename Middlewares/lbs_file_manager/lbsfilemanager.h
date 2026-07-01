#ifndef LBSFILEMANAGER_H
#define LBSFILEMANAGER_H

#include <stdbool.h>
#include <stdint.h>

#define APP_MONITOR_FW_VERSION 108U

void run_python(const char *name);
void __exitpython(void);
void refreshFwlibInfo(void);
bool returnDownLoadState(void);

#endif
