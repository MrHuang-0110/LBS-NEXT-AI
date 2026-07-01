#include "lbsfilemanager.h"
#include "app_pika_runtime.h"
#include "drv_mem.h"
#include "pikaVM.h"

void __exitpython(void)
{
    pks_vm_exit();
}

void run_python(const char *name)
{
    (void)name;
    if (AppPika_HasBytecode() != 0U)
    {
        (void)AppPika_Start();
    }
}

void refreshFwlibInfo(void)
{
    extern volatile uint32_t spark_version;
    spark_version = APP_MONITOR_FW_VERSION;
}

bool returnDownLoadState(void)
{
    return false;
}
