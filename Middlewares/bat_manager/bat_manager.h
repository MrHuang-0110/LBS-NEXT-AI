#ifndef __BAT_MANAGER_H
#define __BAT_MANAGER_H

#include "string.h"

#define VOLTAGE_CRITICAL_LOW  1.53f    // �ٽ�͵���

#define VOLTAGE_LOW_MIN       1.58f    // �͵�����Сֵ
#define VOLTAGE_LOW_MAX       1.68f    // �͵������ֵ

#define VOLTAGE_MID_MIN       1.68f    // �е�����Сֵ
#define VOLTAGE_MID_MAX       1.78f    // �е������ֵ

#define VOLTAGE_HIGH_MIN      1.78f    // �ߵ�����Сֵ
#define VOLTAGE_HIGH_MAX      1.88f    // �ߵ������ֵ

/* �����ٷֱȣ����ٽ��ѹΪ 0%�������ѹΪ 100% */
#define BAT_VOLTAGE_EMPTY     VOLTAGE_CRITICAL_LOW
#define BAT_VOLTAGE_FULL      VOLTAGE_HIGH_MAX
#define BAT_LOW_PERCENT       20       /* ���ڴ˰ٷֱȣ���������̵��� */

// �������״̬ö��
typedef enum {
    BATTERY_CRITICAL = 0,  // �ٽ�������ػ���
    BATTERY_LOW,           // �͵���
    BATTERY_MID,           // �е���
    BATTERY_HIGH,          // �ߵ���
    BATTERY_INVALID        // ��Ч����
} BatteryLevel;

typedef struct {
    float voltage_buffer[10];  // ��ѹ������
    int buffer_index;          // ����������
    float filtered_voltage;    // �˲���ĵ�ѹ
    BatteryLevel last_level;   // �ϴε���״̬
    unsigned int last_check_time;  // �ϴμ��ʱ��
    int low_battery_count;     // �͵���������������
} BatteryManager;


void battery_manager_init(void);
void update_battery_voltage(float new_voltage);
void check_battery_with_debounce(void);
void battery_check_callback(void *arg);
int calculate_battery_percentage(float voltage, float min_v, float max_v);
float get_bat_filtered_volatge(void);
#endif