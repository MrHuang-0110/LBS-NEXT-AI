#ifndef __UI_MANAGER_H
#define __UI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "tm1640_config.h"

// 矩阵尺寸定义
#define UI_MATRIX_ROWS MATRIX_ROWS
#define UI_MATRIX_COLS MATRIX_COLS

// 图案位顺序定义
// 0: 最低位对应最左列（bit0 = 最左列）
// 1: 最高位对应最左列（bit4 = 最左列，与matrix_ui.c一致）
#ifndef UI_PATTERN_BIT_ORDER
#define UI_PATTERN_BIT_ORDER 1
#endif

// 图案像素反转定义
// 0: 正常（1=亮，0=灭）
// 1: 反转（0=亮，1=灭）
#ifndef UI_PATTERN_INVERT
#define UI_PATTERN_INVERT 0
#endif

// 最大UI项数量
#define UI_MAX_ITEMS 20

// UI项回调函数类型
typedef void (*UI_ItemClickCallback)(uint8_t id, void* user_data);
typedef void (*UI_ItemReleaseCallback)(uint8_t id, void* user_data);

// UI项结构
typedef struct {
    uint8_t id;                         // 项ID，唯一标识符
    const char* name;                   // 项名称（可选，用于调试）
    uint8_t pattern[UI_MATRIX_ROWS];    // 图案数据，每行一个字节，低位在右
    UI_ItemClickCallback onClick;       // 点击（按下）回调
    UI_ItemReleaseCallback onRelease;   // 释放（松开）回调
    void* user_data;                    // 用户数据指针，传递给回调
} UI_Item;

// UI管理器结构
typedef struct {
    UI_Item items[UI_MAX_ITEMS];        // UI项数组
    uint8_t item_count;                 // 当前项数量
    uint8_t current_index;              // 当前选中项索引
    bool key_pressed;                   // 按键是否已按下（用于释放检测）
    uint32_t last_key_check;            // 上次按键检查时间（用于消抖）
} UI_Manager;

// 初始化UI管理器
void ui_manager_init(void);

// 添加UI项
// 参数：项ID，名称，图案数据数组（长度UI_MATRIX_ROWS），点击回调，释放回调，用户数据
// 返回：成功返回true，失败（如ID重复或达到最大数量）返回false
bool ui_manager_add_item(uint8_t id, const char* name, const uint8_t pattern[UI_MATRIX_ROWS],
                         UI_ItemClickCallback onClick, UI_ItemReleaseCallback onRelease, void* user_data);
void ui_manager_add_name(char *fileName);
void ui_manager_clear_name(char *fileName);
// 删除UI项（根据ID）
bool ui_manager_remove_item(uint8_t id);

// 获取当前选中项的ID
uint8_t ui_manager_get_current_id(void);

// 切换到下一个UI项（direction_right: true向右，false向左）
void ui_manager_switch_next(bool direction_right);
void ui_manager_refresh_current(void);
// 更新UI管理器（需要在主循环中调用）
// 此函数检查按键状态并触发相应的回调
void ui_manager_update(void);

// 绘制当前选中项的图案到矩阵显示
void ui_manager_draw_current(void);
void ui_manager_load_draw_current(void);
// 清空所有UI项
void ui_manager_clear_all(void);

// 根据ID查找UI项索引（内部使用，也可公开）
int8_t ui_manager_find_index_by_id(uint8_t id);
int8_t ui_manager_find_name_by_id(char *name);
// 获取UI项数量
uint8_t ui_manager_get_item_count(void);

// 检查UI管理器是否为空
bool ui_manager_is_empty(void);

// 通过ID设置当前选中项
bool ui_manager_set_current_by_id(uint8_t id);

// 通过ID获取UI项（只读，返回NULL如果不存在）
const UI_Item* ui_manager_get_item_by_id(uint8_t id);

// 获取当前选中项（只读）
const UI_Item* ui_manager_get_current_item(void);


void onClik(uint8_t id, void* user_data);
void onRelease(uint8_t id,void*user_data);
void ui_display_pattern(const uint8_t pattern[UI_MATRIX_ROWS]);
void ui_display_load_pattern(const uint8_t pattern[UI_MATRIX_ROWS]);
void ui_fileUI_Iteam(void);
#endif /* __UI_MANAGER_H */
