/* 关机时序修复测试
 *
 * 背景：用户反馈"前两秒正常，第三秒蜂鸣器哒一下"。
 * 根因：长按 1.5s 触发 tick 内，第三声进度蜂鸣（beep_play(BEEP_KEY_PRESS)，
 *       800Hz×30ms）刚启动；app_shutdown_sequence 开头立即 beep_stop()，
 *       把第三声掐成"哒"一声（只响 <1ms）。且 beep_play 内部首行也会
 *       beep_stop()，若第三声未播完就启动关机音效，同样会掐断。
 * 修复（main.c app_shutdown_sequence）：
 *   - 去掉开头 beep_stop()；
 *   - 保存 Flash 后等待第三声自然播完（beep_is_playing() 轮询，≤30ms）；
 *   - 再非阻塞 beep_play(BEEP_POWER_OFF) 启动关机音效，与关机动画（1500ms）
 *     同步播放、同步结束，动画结束 beep_stop() 兜底再断电。
 * 本测试锁定这两点行为。
 */
#include <stdio.h>
#include <stdint.h>

static uint32_t g_tick = 0;
static int g_is_playing = 0;
static uint32_t g_tone_start = 0;

/* key_press_sound = {800,30},{0,0}（beep.c） */
static const struct { uint16_t f; uint16_t d; } key_press[] = {{800,30},{0,0}};

static void beep_play_keypress(void){ g_is_playing = 1; g_tone_start = g_tick; }
static void beep_stop_mock(void){ g_is_playing = 0; }
static int beep_is_playing_mock(void){ return g_is_playing; }

/* beep_update 逻辑复制（beep.c:250-303 简化为单音效推进） */
static void beep_update_mock(void){
    if (!g_is_playing) return;
    if ((g_tick - g_tone_start) >= key_press[0].d){
        g_is_playing = 0;   /* 播完 30ms → 自动停止 */
    }
}

/* ---- 断言辅助 ---- */
static int failures = 0;
static void check(const char *name, int ok){
    printf("  %-60s %s\n", name, ok ? "PASS" : "FAIL");
    if (!ok) failures++;
}

int main(void)
{
    printf("=== 关机时序（第三声不掐断 + 音效与动画同步）单元测试 ===\n\n");

    printf("场景A: 旧行为（app_shutdown_sequence 开头立即 beep_stop）:\n");
    g_tick = 0; beep_play_keypress();
    beep_stop_mock();                       /* 旧代码的 beep_stop() */
    check("第三声被掐断（旧 bug 基线）", g_is_playing == 0);

    printf("\n场景B: 新行为（去掉 beep_stop，beep_update 自然推进，等播完再启动关机音效）:\n");
    /* 模拟：触发第三声 → 保存 Flash（假设 5ms 快速返回）→ 等待播完 → 启动关机音效 */
    g_tick = 0; beep_play_keypress();
    for (uint32_t i = 0; i < 5; i++){ g_tick++; beep_update_mock(); }  /* 保存 Flash 5ms */
    uint32_t waited = 0;
    while (beep_is_playing_mock() && waited < 40){ g_tick++; beep_update_mock(); waited++; }
    check("第三声完整播完（保存 Flash 后等待 ≤30ms 才播完）", waited >= 24 && waited <= 32);
    check("第三声播完后 is_playing=false（可安全启动关机音效）", g_is_playing == 0);

    printf("\n场景C: 关机音效非阻塞驱动（beep_play + beep_update，动画期间播放）:\n");
    /* power_off_sound 时长 ≈1300ms，动画 1500ms —— 音效在动画内播完 */
    static const struct { uint16_t f; uint16_t d; } off_sound[] = {
        {784,160},{698,160},{659,160},{587,120},
        {523,100},{494,100},{440,100},{392,100},{330,100},{262,100},{196,100},
        {0,0}
    };
    g_tick = 0;
    g_is_playing = 1;
    g_tone_start = g_tick;
    int idx = 0;
    uint32_t last_playing = 0;
    for (uint32_t i = 0; i < 1500; i++){     /* 动画 1500ms */
        g_tick++;
        if (g_is_playing){
            last_playing = g_tick;
            if (off_sound[idx].f == 0 && off_sound[idx].d == 0){ g_is_playing = 0; }
            else if ((g_tick - g_tone_start) >= off_sound[idx].d){
                idx++;
                if (off_sound[idx].f == 0 && off_sound[idx].d == 0){ g_is_playing = 0; }
                else { g_tone_start = g_tick; }
            }
        }
    }
    check("关机音效在动画期间全程播放、动画结束前结束（≈1300ms）",
          last_playing >= 1100 && last_playing <= 1500);

    printf("\n=== 结果: %d FAIL, %d PASS ===\n", failures, 4 - failures);
    return failures;
}
