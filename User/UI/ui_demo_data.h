/**
 * @file ui_demo_data.h
 * @brief G题串口屏演示用模拟测量数据。
 *
 * 本模块只负责在尚未接入真实算法结果时，为串口屏提供符合题意的
 * 周期波形和频谱。真实测量接入后，可由 UI_Controller_Set... 接口替代。
 */
#ifndef UI_DEMO_DATA_H
#define UI_DEMO_DATA_H

#include "ui_types.h"

#include <stdint.h>

typedef struct
{
    const uint8_t *wave_one_period;
    const uint8_t *wave_three_period;
    uint16_t wave_one_count;
    uint16_t wave_three_count;

    const uint8_t *spectrum;
    uint16_t spectrum_count;

    ui_wave_measurement_t wave;
    ui_spectrum_measurement_t spectrum_measurement;
} ui_demo_profile_t;

/** Build all three G-question demonstration profiles. Call once at boot. */
void UI_Demo_Data_Init(void);

/** Get the simulation profile corresponding to Ua, Ub or recovered U. */
const ui_demo_profile_t *UI_Demo_Data_GetProfile(ui_requirement_t requirement);

#endif
