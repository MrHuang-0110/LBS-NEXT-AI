#ifndef __BEEP_H
#define __BEEP_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

// 蜂鸣器音效类型
typedef enum {
    BEEP_SILENCE = 0,      // 静音
    BEEP_POWER_ON,         // 开机音效
    BEEP_POWER_OFF,        // 关机音效
    BEEP_UI_TRANSITION,    // UI切换音效
	  BEEP_UI_RETURN,        // UI返回
    BEEP_KEY_PRESS,        // 按键按下音效
    BEEP_KEY_RELEASE,      // 按键释放音效
    BEEP_ERROR,            // 错误提示音效
    BEEP_NOTICE,           // 通知提示音效
    BEEP_TYPE_COUNT
} BeepSoundType;

// 单个音调定义（频率+持续时间）
typedef struct {
    uint16_t freq_hz;      // 频率（Hz），0表示静音
    uint16_t duration_ms;  // 持续时间（毫秒）
} BeepTone;

// 蜂鸣器状态
typedef struct {
    bool is_playing;           // 是否正在播放
    BeepSoundType current_sound; // 当前播放的音效类型
    uint8_t tone_index;        // 当前音调索引
    uint32_t tone_start_time;  // 当前音调开始时间（系统tick）
    uint8_t volume;            // 音量（0-100）
} BeepState;

// 初始化蜂鸣器（TIM5, PA1）
void beep_init(void);

// 播放指定音效
void beep_play(BeepSoundType sound_type);

// 停止播放
void beep_stop(void);

// 设置音量（0-100）
void beep_set_volume(uint8_t volume);

// 获取当前音量
uint8_t beep_get_volume(void);

// 更新蜂鸣器状态（需要在主循环中定期调用）
void beep_update(void *arg);

// 检查是否正在播放
bool beep_is_playing(void);

// 便捷函数：常用音效
void beep_play_power_on(void);
void beep_play_power_off(void);
void beep_play_ui_transition(void);
void beep_play_ui_return(void);
void beep_play_key_press(void);
void beep_play_key_release(void);
void beep_play_error(void);
void beep_play_notice(void);

// 播放旋律（阻塞式）
// frequencies: 频率字符串，逗号分隔的频率列表，例如 "523,659,784"
// beat_ms: 每个频率的持续时间（毫秒）
void beep_play_melody(const char* frequencies, uint16_t beat_ms);

// 钢琴音符定义
typedef enum {
    NOTE_C = 0,
    NOTE_C_SHARP,
    NOTE_D,
    NOTE_D_SHARP,
    NOTE_E,
    NOTE_F,
    NOTE_F_SHARP,
    NOTE_G,
    NOTE_G_SHARP,
    NOTE_A,
    NOTE_A_SHARP,
    NOTE_B,
    NOTE_COUNT
} NoteName;

// 八度范围 (0-8, 其中4是中央C所在的八度)
#define OCTAVE_MIN 0
#define OCTAVE_MAX 8

// 获取指定音符和八度的频率
uint16_t beep_get_note_frequency(NoteName note, uint8_t octave);

// 播放钢琴音符旋律（阻塞式）
// notes: 音符字符串，逗号分隔的音符列表，例如 "C4,E4,G4"
// beat_ms: 每个音符的持续时间（毫秒）
void beep_play_piano_melody(const char* notes, uint16_t beat_ms);
void beep_play_shutdown_melody_blocking(void);

#endif /* __BEEP_H */
