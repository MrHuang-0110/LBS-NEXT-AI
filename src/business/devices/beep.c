#include "beep.h"
#include <string.h>
#include <ctype.h>
#include "motor.h"
#include "PikaVM.h"
// 系统时钟频率（Hz）
#define SYSTEM_CLOCK_FREQ  120000000UL

// TIM5预分频器，将时钟降到1MHz
#define TIM5_PRESCALER     119      // 72MHz / (71+1) = 1MHz

// 全局状态
static BeepState beep_state = {0};

// 钢琴频率表 (MIDI音符21-108, 对应A0到C8)
static const uint16_t piano_frequencies[] = {
    28, 29, 31, 33, 35, 37, 39, 41,
    44, 46, 49, 52, 55, 58, 62, 65,
    69, 73, 78, 82, 87, 92, 98, 104,
    110, 117, 123, 131, 139, 147, 156, 165,
    175, 185, 196, 208, 220, 233, 247, 262,
    277, 294, 311, 330, 349, 370, 392, 415,
    440, 466, 494, 523, 554, 587, 622, 659,
    698, 740, 784, 831, 880, 932, 988, 1047,
    1109, 1175, 1245, 1319, 1397, 1480, 1568, 1661,
    1760, 1865, 1976, 2093, 2217, 2349, 2489, 2637,
    2794, 2960, 3136, 3322, 3520, 3729, 3951, 4186,
};

// 音效数据表
// 每个音效是一系列音调，以{0,0}结束

// 开机音效：上升音阶，欢快的启动音，与动画帧同步（5个音调对应5帧）
static const BeepTone power_on_sound[] = {
    {523, 300},   // C5，对应第1帧慢速扩充
    {659, 300},   // E5，对应第2帧慢速扩充
    {784, 100},   // G5，对应第3帧快速填充
    {1047, 100},  // C6，对应第4帧快速填充
    {1319, 100},  // E6，对应第5帧全亮
    {0, 0}        // 结束标记
};

// 关机音效：下降音阶，与关机动画（约 1.5s：闪烁 480ms + 全亮 120ms + 逐个熄灭 900ms）同步
static const BeepTone power_off_sound[] = {
    {784, 160},   // G5  闪烁阶段（6×80ms≈480ms，前 3 段）
    {698, 160},   // F5
    {659, 160},   // E5
    {587, 120},   // D5  全亮（120ms）
    {523, 100},   // C5  逐个熄灭阶段（9×100ms=900ms，后 5 段）
    {494, 100},   // B4
    {440, 100},   // A4
    {392, 100},   // G4
    {330, 100},   // E4
    {262, 100},   // C4
    {196, 100},   // G3
    {0, 0}        // 结束标记
};

// UI切换音效：短促双音提示
static const BeepTone ui_transition_sound[] = {
    {800, 50},    // 高音提示
    {0, 20},      // 短暂静音
    {600, 50},    // 低音确认
    {0, 0}        // 结束标记
};

// UI返回音效
static const BeepTone ui_return_sound[] = {
    {600, 50},    // 高音提示
    {0, 20},      // 短暂静音
    {800, 50},    // 低音确认
    {0, 0}        // 结束标记
};

// 按键按下音效：短促清脆
static const BeepTone key_press_sound[] = {
    {800, 30},    // 短促高频
    {0, 0}        // 结束标记
};

// 按键释放音效：轻微反馈
static const BeepTone key_release_sound[] = {
    {600, 20},    // 较短低频
    {0, 0}        // 结束标记
};

// 错误音效：重复急促音
static const BeepTone error_sound[] = {
    {300, 80},    // 低音警告
    {0, 50},      // 间隔
    {300, 80},    // 重复
    {0, 50},      // 间隔
    {300, 80},    // 再次重复
    {0, 0}        // 结束标记
};

// 通知音效：清晰提示
static const BeepTone notice_sound[] = {
    {1000, 50},   // 高频提示
    {0, 50},      // 间隔
    {1000, 50},   // 重复一次
    {0, 0}        // 结束标记
};

// 根据音效类型获取音调数据
static const BeepTone* get_sound_data(BeepSoundType sound_type) {
    switch(sound_type) {
        case BEEP_POWER_ON:
            return power_on_sound;
        case BEEP_POWER_OFF:
            return power_off_sound;
        case BEEP_UI_TRANSITION:
            return ui_transition_sound;
				case BEEP_UI_RETURN:
					  return ui_return_sound;
        case BEEP_KEY_PRESS:
            return key_press_sound;
        case BEEP_KEY_RELEASE:
            return key_release_sound;
        case BEEP_ERROR:
            return error_sound;
        case BEEP_NOTICE:
            return notice_sound;
        case BEEP_SILENCE:
        default:
            return NULL;
    }
}

// 根据频率计算ARR值（1MHz时钟）
static uint32_t calculate_arr_from_freq(uint16_t freq_hz) {
    if (freq_hz == 0) return 0;
    // ARR = (1MHz / freq_hz) - 1
    return (1000000UL / freq_hz) - 1;
}

// 播放指定频率的音调
static void play_frequency(uint16_t freq_hz) {
    if (freq_hz == 0) {
        // 静音：关闭输出
        TIM5->CCER &= ~TIM_CCER_CC2E;  // 禁用通道2输出
        TIM5->CCR2 = 0;
        return;
    }

    // 计算ARR值
    uint32_t arr = calculate_arr_from_freq(freq_hz);
    if (arr == 0) arr = 1;  // 防止除零

    // 更新定时器参数
    TIM5->ARR = arr;

    // 设置占空比为50%
    TIM5->CCR2 = arr / 2;

    // 启用通道输出
    TIM5->CCER |= TIM_CCER_CC2E;
}

// 初始化蜂鸣器
void beep_init(void) {
    // 1. 使能GPIOA和TIM5时钟
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;

    // 2. 配置PA1为复用推挽输出（TIM5_CH2）
    GPIOA->CRL &= ~(0xF << 4);     // 清除PA1配置
    GPIOA->CRL |= (0xB << 4);      // 复用推挽输出，50MHz

    // 3. 配置TIM5
    // 3.1 设置预分频器（1MHz时钟）
    TIM5->PSC = TIM5_PRESCALER;

    // 3.2 设置自动重装载值（初始1KHz）
    TIM5->ARR = 999;  // 1MHz / 1000 = 1KHz

    // 3.3 配置PWM模式1，通道2
    TIM5->CCMR1 &= ~TIM_CCMR1_OC2M;  // 清除模式位
    TIM5->CCMR1 |= (6 << 12);        // PWM模式1 (OC2M=110)
    TIM5->CCMR1 |= (1 << 11);        // 预装载使能

    // 3.4 初始占空比50%
    TIM5->CCR2 = 500;

    // 3.5 使能自动重装载预装载
    TIM5->CR1 |= TIM_CR1_ARPE;

    // 3.6 启用通道2输出（先禁用，等需要时再启用）
    TIM5->CCER &= ~TIM_CCER_CC2E;

    // 3.7 启动定时器
    TIM5->CR1 |= TIM_CR1_CEN;

    // 4. 初始化状态
    memset(&beep_state, 0, sizeof(beep_state));
    beep_state.volume = 80;  // 默认音量80%
}

// 播放指定音效
void beep_play(BeepSoundType sound_type) {
    if (sound_type >= BEEP_TYPE_COUNT) return;

    // 停止当前播放
    beep_stop();

    // 获取音效数据
    const BeepTone* sound_data = get_sound_data(sound_type);
    if (sound_data == NULL) return;

    // 更新状态
    beep_state.is_playing = true;
    beep_state.current_sound = sound_type;
    beep_state.tone_index = 0;
    beep_state.tone_start_time = HAL_GetTick();

    // 播放第一个音调
    if (sound_data[0].freq_hz != 0 || sound_data[0].duration_ms != 0) {
        play_frequency(sound_data[0].freq_hz);
    } else {
        beep_stop();
    }
}

// 停止播放
void beep_stop(void) {
    // 关闭PWM输出
    TIM5->CCER &= ~TIM_CCER_CC2E;
    TIM5->CCR2 = 0;

    // 重置状态
    beep_state.is_playing = false;
    beep_state.current_sound = BEEP_SILENCE;
    beep_state.tone_index = 0;
}

// 设置音量（0-100）
void beep_set_volume(uint8_t volume) {
    if (volume > 100) volume = 100;
    beep_state.volume = volume;

    // 如果正在播放，更新当前占空比
    if (beep_state.is_playing) {
        uint32_t arr = TIM5->ARR;
        if (arr > 0) {
            // 根据音量调整占空比（线性变化）
            TIM5->CCR2 = (arr * volume) / 100;
        }
    }
}

// 获取当前音量
uint8_t beep_get_volume(void) {
    return beep_state.volume;
}

// 更新蜂鸣器状态
void beep_update(void *arg) {
    if (!beep_state.is_playing) return;

    // 获取当前音效数据
    const BeepTone* sound_data = get_sound_data(beep_state.current_sound);
    if (sound_data == NULL) {
        beep_stop();
        return;
    }

    // 获取当前音调
    const BeepTone* current_tone = &sound_data[beep_state.tone_index];

    // 检查是否到达音调序列末尾
    if (current_tone->freq_hz == 0 && current_tone->duration_ms == 0) {
        beep_stop();
        return;
    }

    // 检查当前音调是否播放完成
    uint32_t current_time = HAL_GetTick();
    uint32_t elapsed_time = current_time - beep_state.tone_start_time;

    if (elapsed_time >= current_tone->duration_ms) {
        // 移动到下一个音调
        beep_state.tone_index++;
        current_tone = &sound_data[beep_state.tone_index];

        // 检查是否还有更多音调
        if (current_tone->freq_hz == 0 && current_tone->duration_ms == 0) {
            beep_stop();
            return;
        }

        // 播放下一个音调
        beep_state.tone_start_time = current_time;
        play_frequency(current_tone->freq_hz);

        // 应用音量设置
        if (current_tone->freq_hz != 0) {
            uint32_t arr = TIM5->ARR;
            if (arr > 0) {
                TIM5->CCR2 = (arr * beep_state.volume) / 100;
            }
        }
    }
}

// 检查是否正在播放
bool beep_is_playing(void) {
    return beep_state.is_playing;
}

// 便捷函数：常用音效
void beep_play_power_on(void) {
    beep_play(BEEP_POWER_ON);
}

void beep_play_power_off(void) {
    beep_play(BEEP_POWER_OFF);
}

void beep_play_ui_transition(void) {
    beep_play(BEEP_UI_TRANSITION);
}
void beep_play_ui_return(void){ 
    beep_play(BEEP_UI_RETURN);
}
void beep_play_key_press(void) {
    beep_play(BEEP_KEY_PRESS);
}

void beep_play_key_release(void) {
    beep_play(BEEP_KEY_RELEASE);
}

void beep_play_error(void) {
    beep_play(BEEP_ERROR);
}

void beep_play_notice(void) {
    beep_play(BEEP_NOTICE);
}

// 播放旋律（阻塞式）
void beep_play_melody(const char* frequencies, uint16_t beat_ms) {
    if (frequencies == NULL || beat_ms == 0) {
        return;
    }

    // 停止任何当前播放
    beep_stop();

    const char* p = frequencies;
    while (*p != '\0') {
        // 跳过空格和逗号
        while (*p == ' ' || *p == ',') p++;
        if (*p == '\0') break;

        // 提取频率数字
        uint16_t freq = 0;
        while (*p >= '0' && *p <= '9') {
            freq = freq * 10 + (*p - '0');
            p++;
        }

        // 播放该频率
        play_frequency(freq);

        // 应用当前音量设置
        if (freq != 0) {
            uint32_t arr = TIM5->ARR;
            if (arr > 0) {
                TIM5->CCR2 = (arr * beep_state.volume) / 100;
            }
        }

        // 阻塞延迟
        HAL_Delay(beat_ms);

        // 跳过当前数字后的字符直到逗号或结尾
        while (*p != ',' && *p != '\0') p++;
    }

    // 播放结束后静音
    play_frequency(0);
}

// 将字符转换为音符值
static int note_char_to_value(char c) {
    // 直接比较，避免依赖toupper
    if (c == 'C' || c == 'c') return 0;
    if (c == 'D' || c == 'd') return 2;
    if (c == 'E' || c == 'e') return 4;
    if (c == 'F' || c == 'f') return 5;
    if (c == 'G' || c == 'g') return 7;
    if (c == 'A' || c == 'a') return 9;
    if (c == 'B' || c == 'b') return 11;
    return -1;
}

// 解析音符字符串（如"C4", "C#4", "Db4"）
// 返回MIDI音符编号，失败返回-1
static int parse_note_string(const char* str) {
    if (str == NULL || str[0] == '\0') {
        return -1;
    }

    // 解析音符字母
    int note_value = note_char_to_value(str[0]);
    if (note_value < 0) {
        return -1;
    }

    int idx = 1;

    // 检查升降号
    if (str[idx] == '#') {
        note_value++;
        idx++;
    } else if (str[idx] == 'b') {
        note_value--;
        idx++;
    }

    // 解析八度数（可选，默认为4）
    int octave = 4; // 默认中央C八度

    if (str[idx] != '\0') {
        // 八度数应该是数字
        if (!isdigit((unsigned char)str[idx])) {
            return -1;
        }

        octave = str[idx] - '0';
        idx++;

        // 检查是否有多位八度数（如10）
        if (isdigit((unsigned char)str[idx])) {
            octave = octave * 10 + (str[idx] - '0');
            idx++;
        }

        // 检查字符串是否完全解析
        if (str[idx] != '\0') {
            return -1;
        }
    }

    // 处理升降号导致的八度变化（必须在解析八度数后进行）
    if (note_value < 0) {
        note_value += 12;
        octave--;  // 例如Cb变为前一个八度的B
    } else if (note_value >= 12) {
        note_value -= 12;
        octave++;  // 例如B#变为下一个八度的C
    }

    // 检查八度范围
    if (octave < 0 || octave > 8) {
        return -1;
    }

    // 计算MIDI音符编号
    // MIDI音符编号 = 音符值 + 12 * (八度 + 1)
    // C0 = 12, C1 = 24, C4 = 60, A4 = 69
    int midi_note = note_value + 12 * (octave + 1);

    // 检查是否在钢琴范围内（21-108对应A0到C8）
    if (midi_note < 21 || midi_note > 108) {
        return -1;
    }

    return midi_note;
}

// 获取指定音符和八度的频率
uint16_t beep_get_note_frequency(NoteName note, uint8_t octave) {
    // MIDI音符编号 = 音符值 + 12 * (八度 + 1)
    int note_value = note; // NoteName枚举值对应音符值
    int midi_note = note_value + 12 * (octave + 1);

    // 检查范围
    if (midi_note < 21 || midi_note > 108) {
        return 0;
    }

    // 使用查表获取频率
    return piano_frequencies[midi_note - 21];
}

// 播放钢琴音符旋律（阻塞式）
void beep_play_piano_melody(const char* notes, uint16_t beat_ms) {
    if (notes == NULL || beat_ms == 0) {
        return;
    }

    // 停止任何当前播放
    beep_stop();

    const char* p = notes;

    while (*p != '\0') {
        // 跳过空格和逗号
        while (*p == ' ' || *p == ',') p++;
        if (*p == '\0') break;

        // 找到当前音符的结束位置（逗号或字符串结束）
        const char* note_start = p;
        while (*p != ',' && *p != '\0' && *p != ' ') p++;

        // 提取音符子字符串
        int note_len = p - note_start;
        if (note_len == 0) {
            continue;
        }

        // 复制音符字符串以便解析
        char note_buf[8];
        if (note_len >= sizeof(note_buf)) {
            note_len = sizeof(note_buf) - 1;
        }
        strncpy(note_buf, note_start, note_len);
        note_buf[note_len] = '\0';

        // 解析音符
        int midi_note = parse_note_string(note_buf);
        if (midi_note >= 21 && midi_note <= 108) {
            // 从表中获取频率
            uint16_t freq_hz = piano_frequencies[midi_note - 21];
					extern void usb_printf(char *fmt, ...);
					usb_printf("hz:%d\r\n",freq_hz);
            // 播放该频率
            play_frequency(freq_hz);

            // 应用当前音量设置
            if (freq_hz != 0) {
                uint32_t arr = TIM5->ARR;
                if (arr > 0) {
                    TIM5->CCR2 = (arr * beep_state.volume) / 100;
                }
            }

            // 阻塞延迟
            motor_delay_exit(beat_ms);
        } else {
            // 解析失败，跳过
        }

        // 检查VM信号
        if (VMSignal_getCtrl() == VM_SIGNAL_CTRL_EXIT) {
            break;
        }

        // 跳过当前音符后的分隔符
        while (*p == ' ' || *p == ',') p++;
    }

    // 播放结束后静音
    play_frequency(0);
}
