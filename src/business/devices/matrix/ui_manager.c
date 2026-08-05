#include "ui_manager.h"
#include "display.h"
#include "key.h"
#include "beep.h"
#include "blue.h"
#include "animation.h"
#include "exfuns.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include "stdlib.h"
#include "lbsfilemanager.h"
// 全局UI管理器实例
static UI_Manager ui_manager = {0};

// 内部函数声明
 
static bool ui_check_key_left(void);
static bool ui_check_key_right(void);
static void pattern_to_buffer(const uint8_t pattern[UI_MATRIX_ROWS], uint8_t buffer[MATRIX_ROWS][MATRIX_COLS]);

static const char itemName[10][7] = {
 {"0.o"},
 {"1.o"},
 {"2.o"},
 {"3.o"},
 {"4.o"},
 {"5.o"},
 {"6.o"},
 {"7.o"},
 {"8.o"},
 {"9.o"}
};
static const uint8_t itemlogo[10][7] = {
  {0b01110,
	 0b10001,
	 0b10011,
	 0b10101,
	 0b11001,
	 0b10001,
	 0b01110
	},/*0*/
	{
	 0b00100,
	 0b01100,
	 0b00100,
	 0b00100,
	 0b00100,
	 0b00100,
	 0b11111
	},/*1*/
	{
	 0b01110,
	 0b10001,
	 0b00001,
	 0b00010,
	 0b00100,
	 0b01000,
	 0b11111
	},/*2*/
	{
	 0b11111,
	 0b00010,
	 0b00100,
	 0b00010,
	 0b00001,
	 0b10001,
	 0b01110
	},/*3*/
	{
	 0b00010,
	 0b00110,
	 0b01010,
	 0b10010,
	 0b11111,
	 0b00010,
	 0b00010
	},/*4*/
	{
	 0b11111,
	 0b10000,
	 0b11110,
	 0b00001,
	 0b00001,
	 0b10001,
	 0b01110
	},/*5*/	
	{
	 0b01110,
	 0b01000,
	 0b10000,
	 0b11110,
	 0b10001,
	 0b10001,
	 0b01110
	},/*6*/		
	{
	 0b11111,
	 0b00001,
	 0b00001,
	 0b00100,
	 0b01000,
	 0b01000,
	 0b01000
	},/*7*/	
	{
	 0b01110,
	 0b10001,
	 0b10001,
	 0b01110,
	 0b10001,
	 0b10001,
	 0b01110
	},/*8*/
	{
	 0b01110,
	 0b10001,
	 0b10001,
	 0b01111,
	 0b00001,
	 0b10001,
	 0b01110
	}/*9*/
};
extern void usb_printf(char* fmt,...); 
void onClik(uint8_t id, void* user_data)
{ 
  // usb_printf("middle key touch\r\n");
	 
}
void onRelease(uint8_t id,void*user_data)
{ 
//  usb_printf("middle key release\r\n");
}
// 初始化UI管理器
/* ===== Task 16: exfuns file-transfer UI callback (registered by business layer) ===== */
static void exfuns_ui_event_handler(ExfunsUiEvent event, const char *name)
{
    switch (event) {
    case EXFUNS_UI_EVT_ADD_NAME:
        if (name != NULL) {
            ui_manager_add_name((char *)name);
        }
        break;
    case EXFUNS_UI_EVT_FILE_OK:
    case EXFUNS_UI_EVT_FILE_ERROR:
        /* touchFileOKCallBack()/touchFileErrorCallBack() were never implemented;
           keep original runtime behavior (no-op) */
        break;
    default:
        break;
    }
}

void ui_manager_init(void)
{
    memset(&ui_manager, 0, sizeof(UI_Manager));
    ui_manager.item_count = 0;
    ui_manager.current_index = 0;
    ui_manager.key_pressed = false;
    ui_manager.last_key_check = HAL_GetTick();
    Exfuns_RegisterUiHandler(exfuns_ui_event_handler);
}
bool ends_with_strstr(const char* str) {
    if (!str || strlen(str) < 2) return false;
    
    const char* dot = strstr(str, ".o");
    if (!dot) return false;
    
    // 检查".o"是否是字符串的结尾
    return strlen(dot) == 2;
}
// 添加UI项
bool ui_manager_add_item(uint8_t id, const char* name, const uint8_t pattern[UI_MATRIX_ROWS],
                         UI_ItemClickCallback onClick, UI_ItemReleaseCallback onRelease, void* user_data)
{
    // 检查是否达到最大数量
    if (ui_manager.item_count >= UI_MAX_ITEMS) {
        return false;
    }

    // 检查ID是否已存在
    if (ui_manager_find_index_by_id(id) >= 0) {
        return false;
    }

    UI_Item* item = &ui_manager.items[ui_manager.item_count];
    item->id = id;
    item->name = name;
    memcpy(item->pattern, pattern, UI_MATRIX_ROWS);
    item->onClick = onClick;
    item->onRelease = onRelease;
    item->user_data = user_data;

    ui_manager.item_count++;
    return true;
}

// 删除UI项（根据ID）
bool ui_manager_remove_item(uint8_t id)
{
    int8_t index = ui_manager_find_index_by_id(id);
    if (index < 0) {
        return false;
    }

    // 将后面的项向前移动
    for (uint8_t i = index; i < ui_manager.item_count - 1; i++) {
        ui_manager.items[i] = ui_manager.items[i + 1];
    }

    ui_manager.item_count--;

    // 如果删除的是当前选中项，且当前索引超出范围，则调整索引
    if (ui_manager.current_index >= ui_manager.item_count) {
        if (ui_manager.item_count > 0) {
            ui_manager.current_index = ui_manager.item_count - 1;
        } else {
            ui_manager.current_index = 0;
        }
    }

    return true;
}

// 获取当前选中项的ID
uint8_t ui_manager_get_current_id(void)
{
    if (ui_manager.item_count == 0) {
        return 0;
    }
    return ui_manager.items[ui_manager.current_index].id;
}

void ui_manager_refresh_current(void)
{ 
	   ui_manager_load_draw_current();
	   beep_play_ui_return();
	   uint32_t speed_ms = UI_TRANSITION_DURATION_MS / MATRIX_ROWS;
     if (speed_ms < 10) speed_ms = 10; // 最小速度
     animation_vertical_scroll(DIRECTION_DOWN, speed_ms);
     // 等待动画完成（阻塞式，类似ui_manager_switch_next）
     uint32_t animation_start_time = HAL_GetTick();
     while (HAL_GetTick() - animation_start_time < UI_TRANSITION_DURATION_MS) {
            // 更新动画
            animation_update();
            HAL_Delay(10); // 短暂延时
     }
 
    ui_manager_draw_current();
}

void ui_manager_add_name(char *fileName)
{ 
	 int num = addUIteam(fileName);
	 if(num!=-1)
	 { 
		  const char *Name = itemName[num];
		  const uint8_t *logo = itemlogo[num];
			ui_manager_add_item(num, Name, logo, onClik, onRelease, NULL); 
		  int8_t index = ui_manager_find_index_by_id(num);
		  if(index!=(-1))
		  {
		     ui_manager.current_index = index;
				 ui_manager_draw_current(); 
		  }		  
	 }
}
 
void ui_manager_clear_name(char *fileName)
{ 
   ;
}
void ui_fileUI_Iteam(void)
{	 char fileName[16];
	 ui_manager_clear_all();

	  uint8_t remote_logo[] = {0x0E, 0x1F, 0x1F, 0x0E, 0x04, 0x04, 0x1F};	
    ui_manager_add_item(11, "remote.o", remote_logo, onClik, onRelease, NULL);	 
    uint8_t blue_log[] = {0x06, 0x15, 0x0D, 0x06, 0x0D, 0x15, 0x06};
    ui_manager_add_item(12, "blue", blue_log, onClik, onRelease, NULL);
    uint8_t p_log[] = {0x0E, 0x09, 0x09, 0x0E, 0x08, 0x08, 0x08};
    ui_manager_add_item(10, "Pauto", p_log, onClik, onRelease, NULL);		
	 uint8_t file_sum = get_root_files_list();
	 for(uint8_t i = 0; i < file_sum; i++)
	 {memset(fileName, 0, sizeof(fileName));
		if(cpy_fileName(i,fileName))
		{ 
			int num = addUIteam(fileName);
			if(num!=-1)
			{ 
				const char *Name = itemName[num];
				const uint8_t *logo = itemlogo[num];
				ui_manager_add_item(num, Name, logo, onClik, onRelease, NULL); 
			}
		}
	}	 
}
 
void ui_manager_switch_next(bool direction_right)
{
    if (ui_manager.item_count <= 1) {
        return;
    }

    // 计算下一个索引
    uint8_t next_index;
    if (direction_right) {
        next_index = ui_manager.current_index + 1;
        if (next_index >= ui_manager.item_count) {
            next_index = 0;
        }
    } else {
        if (ui_manager.current_index == 0) {
            next_index = ui_manager.item_count - 1;
        } else {
            next_index = ui_manager.current_index - 1;
        }
    }

    // 准备缩放动画
    uint8_t current_buffer[MATRIX_ROWS][MATRIX_COLS];
    uint8_t next_buffer[MATRIX_ROWS][MATRIX_COLS];

    // 转换当前图案和新图案为缓冲区
    pattern_to_buffer(ui_manager.items[ui_manager.current_index].pattern, current_buffer);
    pattern_to_buffer(ui_manager.items[next_index].pattern, next_buffer);

    // 设置动画缓冲区
    animation_set_current_frame_buffer(current_buffer);
    animation_set_next_frame_buffer(next_buffer);

    // 播放缩放动画（300毫秒），带滑动效果
    AnimationDirection anim_direction = direction_right ? DIRECTION_RIGHT : DIRECTION_LEFT;
    animation_ui_zoom(anim_direction, UI_TRANSITION_DURATION_MS);

    // 基于时间等待动画完成（300ms）
    uint32_t animation_start_time = HAL_GetTick();
    while (HAL_GetTick() - animation_start_time < UI_TRANSITION_DURATION_MS) {
        // 更新动画（通常由display_update处理，但这里手动调用以确保动画进行）
        //animation_update();
        HAL_Delay(10); // 短暂延时
    }
    // 动画已经完成，不需要额外停止（UI_ZOOM动画完成后不清屏）

    // 动画完成后，更新当前索引
    ui_manager.current_index = next_index;

    // 确保显示新图案（动画最后一帧应该已经显示完整图案）
    //ui_manager_draw_current();
}

// 更新UI管理器（需要在主循环中调用）
void ui_manager_update(void)
{
	  static uint32_t last_blink_blue;
    if (ui_manager.item_count == 0) {
        return;
    }
    
		if(strcmp(ui_manager_get_current_item()->name,"blue") == 0)
		{
			  if((HAL_GetTick() - last_blink_blue) > 500)
				{
				   blue_logo_blinke();
					 last_blink_blue = HAL_GetTick();
				} 
		}
		
    // 按键消抖检查
    uint32_t current_time = HAL_GetTick();
    if (current_time - ui_manager.last_key_check < 10) { // 10ms消抖，更敏感
        return;
    }
    ui_manager.last_key_check = current_time;
 
    bool left_now = ui_check_key_left();
    bool right_now = ui_check_key_right();
    
 
    if (left_now) {
			  beep_play_key_press();
        ui_manager_switch_next(false); // 向左切换（下降沿）
    }
    if (right_now) {
			  beep_play_key_press();
        ui_manager_switch_next(true);  // 向右切换（下降沿）
    }		
}

// 绘制当前选中项的图案到矩阵显示
void ui_manager_draw_current(void)
{
    if (ui_manager.item_count == 0) {
        return;
    }
    UI_Item* current = &ui_manager.items[ui_manager.current_index];
    ui_display_pattern(current->pattern);
}

void ui_manager_load_draw_current(void)
{ 
     if (ui_manager.item_count == 0) {
        return;
    }
    UI_Item* current = &ui_manager.items[ui_manager.current_index];
    ui_display_load_pattern(current->pattern);  
}
// 清空所有UI项
void ui_manager_clear_all(void)
{
    ui_manager.item_count = 0;
    ui_manager.current_index = 0;
    ui_manager.key_pressed = false;
}

// 根据ID查找UI项索引
int8_t ui_manager_find_index_by_id(uint8_t id)
{
    for (uint8_t i = 0; i < ui_manager.item_count; i++) {
        if (ui_manager.items[i].id == id) {
            return i;
        }
    }
    return -1;
}

int8_t ui_manager_find_name_by_id(char *name)
{
    for (uint8_t i = 0; i < ui_manager.item_count; i++) {
				if(strcmp(ui_manager.items[i].name,name) == 0)
				{
				   return i;
				}
    }
    return -1;
}

void ui_display_load_pattern(const uint8_t pattern[UI_MATRIX_ROWS])
{ 
 // 使用display_set_pixel直接绘制图案，避免缓冲区转换问题
    for (uint8_t row = 0; row < UI_MATRIX_ROWS; row++) {
        uint8_t row_data = pattern[row];
        for (uint8_t col = 0; col < UI_MATRIX_COLS; col++) {
            uint8_t bit_pos;
            #if UI_PATTERN_BIT_ORDER == 0
                // 最低位对应最左列（bit0 = 最左列）
                bit_pos = col;
            #else
                // 最高位对应最左列（bit4 = 最左列，与matrix_ui.c一致）
                bit_pos = UI_MATRIX_COLS - 1 - col;
            #endif
            bool pixel_state = (row_data >> bit_pos) & 0x01;
            #if UI_PATTERN_INVERT == 1
                pixel_state = !pixel_state;
            #endif
            if (pixel_state) {
                display_load_pixel(col, row, true);
            }
        }
    }  
}
// 内部函数：显示图案
void ui_display_pattern(const uint8_t pattern[UI_MATRIX_ROWS])
{
    // 清屏
    display_clear();

    // 使用display_set_pixel直接绘制图案，避免缓冲区转换问题
    for (uint8_t row = 0; row < UI_MATRIX_ROWS; row++) {
        uint8_t row_data = pattern[row];
        for (uint8_t col = 0; col < UI_MATRIX_COLS; col++) {
            uint8_t bit_pos;
            #if UI_PATTERN_BIT_ORDER == 0
                // 最低位对应最左列（bit0 = 最左列）
                bit_pos = col;
            #else
                // 最高位对应最左列（bit4 = 最左列，与matrix_ui.c一致）
                bit_pos = UI_MATRIX_COLS - 1 - col;
            #endif
            bool pixel_state = (row_data >> bit_pos) & 0x01;
            #if UI_PATTERN_INVERT == 1
                pixel_state = !pixel_state;
            #endif
            if (pixel_state) {
                display_set_pixel(col, row, true);
            }
        }
    }
}



// 内部函数：检查左键是否按下
static bool ui_check_key_left(void)
{
    // KEY1: 0表示按下，1表示松开（根据key.h定义）
    // 如果key.h未定义KEY1，用户需要修改此函数
    #ifdef KEY1
    // 简单检测：按下为低电平
    return (KEY1 == 0);
    #else
    return false;
    #endif
}

// 内部函数：检查右键是否按下
static bool ui_check_key_right(void)
{
    // WK_UP: 0表示按下，1表示松开（根据key.h定义）
    #ifdef WK_UP
    return (WK_UP == 0);
    #else
    return false;
    #endif
}

// 获取UI项数量
uint8_t ui_manager_get_item_count(void)
{
    return ui_manager.item_count;
}

// 检查UI管理器是否为空
bool ui_manager_is_empty(void)
{
    return (ui_manager.item_count == 0);
}

// 通过ID设置当前选中项
bool ui_manager_set_current_by_id(uint8_t id)
{
    int8_t index = ui_manager_find_index_by_id(id);
    if (index < 0) {
        return false;
    }
    ui_manager.current_index = index;
    ui_manager_draw_current();
    return true;
}

// 通过ID获取UI项（只读，返回NULL如果不存在）
const UI_Item* ui_manager_get_item_by_id(uint8_t id)
{
    int8_t index = ui_manager_find_index_by_id(id);
    if (index < 0) {
        return NULL;
    }
    return &ui_manager.items[index];
}

// 获取当前选中项（只读）
const UI_Item* ui_manager_get_current_item(void)
{
    if (ui_manager.item_count == 0) {
        return NULL;
    }
    return &ui_manager.items[ui_manager.current_index];
}

// 将图案数据转换为缓冲区
static void pattern_to_buffer(const uint8_t pattern[UI_MATRIX_ROWS], uint8_t buffer[MATRIX_ROWS][MATRIX_COLS])
{
    for (uint8_t row = 0; row < UI_MATRIX_ROWS; row++) {
        uint8_t row_data = pattern[row];
        for (uint8_t col = 0; col < UI_MATRIX_COLS; col++) {
            uint8_t bit_pos;
            #if UI_PATTERN_BIT_ORDER == 0
                // 最低位对应最左列（bit0 = 最左列）
                bit_pos = col;
            #else
                // 最高位对应最左列（bit4 = 最左列，与matrix_ui.c一致）
                bit_pos = UI_MATRIX_COLS - 1 - col;
            #endif
            bool pixel_state = (row_data >> bit_pos) & 0x01;
            #if UI_PATTERN_INVERT == 1
                pixel_state = !pixel_state;
            #endif
            buffer[row][col] = pixel_state ? 1 : 0;
        }
    }
}
