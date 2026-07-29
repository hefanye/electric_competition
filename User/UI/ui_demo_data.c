/**
 * @file ui_demo_data.c
 * @brief G题要求对应的模拟波形与频谱数据。
 *
 * Ua: 100 kHz 基波及二、三次谐波；
 * Ub: 150 kHz 基波及二、三次谐波（最高 450 kHz，满足 500 kHz 范围）；
 * U : 120 kHz 的去干扰后恢复信号及其谐波。
 *
 * 波形值是 0~255 的相对显示高度，不是电压 ADC 码。
 */
#include "ui_demo_data.h"

#define DEMO_ONE_POINTS      64U
#define DEMO_THREE_POINTS    (DEMO_ONE_POINTS * 3U)
#define DEMO_SPECTRUM_POINTS 64U

static const uint8_t s_sine[DEMO_ONE_POINTS] =
{
    128, 140, 153, 165, 177, 188, 199, 210,
    218, 226, 234, 240, 245, 250, 253, 255,
    255, 255, 253, 250, 245, 240, 234, 226,
    218, 210, 199, 188, 177, 165, 153, 140,
    128, 115, 102,  90,  78,  67,  56,  45,
     37,  29,  21,  15,  10,   5,   2,   0,
      0,   0,   2,   5,  10,  15,  21,  29,
     37,  45,  56,  67,  78,  90, 102, 115
};

static uint8_t s_wave_one[3][DEMO_ONE_POINTS];
static uint8_t s_wave_three[3][DEMO_THREE_POINTS];
static uint8_t s_spectrum[3][DEMO_SPECTRUM_POINTS];
static ui_demo_profile_t s_profiles[3];
static uint8_t s_initialized;

static uint8_t clamp_u8(int32_t value)
{
    if (value < 0) {
        return 0U;
    }
    if (value > 255) {
        return 255U;
    }
    return (uint8_t)value;
}

static void build_wave(uint8_t index, int32_t fundamental_gain,
                       int32_t harmonic2_gain, int32_t harmonic3_gain)
{
    uint16_t i;

    for (i = 0U; i < DEMO_ONE_POINTS; ++i) {
        int32_t s1 = (int32_t)s_sine[i] - 128;
        int32_t s2 = (int32_t)s_sine[(i * 2U) % DEMO_ONE_POINTS] - 128;
        int32_t s3 = (int32_t)s_sine[(i * 3U) % DEMO_ONE_POINTS] - 128;
        int32_t value = 128 + (fundamental_gain * s1 +
                               harmonic2_gain * s2 +
                               harmonic3_gain * s3) / 128;
        s_wave_one[index][i] = clamp_u8(value);
        s_wave_three[index][i] = s_wave_one[index][i];
        s_wave_three[index][i + DEMO_ONE_POINTS] = s_wave_one[index][i];
        s_wave_three[index][i + 2U * DEMO_ONE_POINTS] = s_wave_one[index][i];
    }
}

static void build_spectrum(uint8_t index, uint8_t peak1, uint8_t peak2,
                           uint8_t peak3)
{
    uint16_t i;

    for (i = 0U; i < DEMO_SPECTRUM_POINTS; ++i) {
        s_spectrum[index][i] = 4U;
    }

    s_spectrum[index][peak1] = 255U;
    s_spectrum[index][peak2] = 82U;
    s_spectrum[index][peak3] = 38U;

    if (peak1 > 0U) {
        s_spectrum[index][peak1 - 1U] = 35U;
    }
    if (peak1 + 1U < DEMO_SPECTRUM_POINTS) {
        s_spectrum[index][peak1 + 1U] = 35U;
    }
}

void UI_Demo_Data_Init(void)
{
    if (s_initialized != 0U) {
        return;
    }

    /* Ua: 基波100 kHz + 二、三次谐波。 */
    build_wave(UI_REQUIREMENT_UA, 78, 22, 10);
    build_spectrum(UI_REQUIREMENT_UA, 12U, 24U, 36U);
    s_profiles[UI_REQUIREMENT_UA].wave = (ui_wave_measurement_t){180U, 58U, 100000000U};
    s_profiles[UI_REQUIREMENT_UA].spectrum_measurement =
        (ui_spectrum_measurement_t){{100000000U, 200000000U, 300000000U}, {70U, 18U, 8U}};

    /* Ub: 最高谐波450 kHz，仍处于题设 10~500 kHz 范围。 */
    build_wave(UI_REQUIREMENT_UB, 66, 24, 12);
    build_spectrum(UI_REQUIREMENT_UB, 16U, 32U, 48U);
    s_profiles[UI_REQUIREMENT_UB].wave = (ui_wave_measurement_t){155U, 51U, 150000000U};
    s_profiles[UI_REQUIREMENT_UB].spectrum_measurement =
        (ui_spectrum_measurement_t){{150000000U, 300000000U, 450000000U}, {62U, 19U, 9U}};

    /* U: 加入1 MHz干扰后，滤波恢复得到的有效信号；显示结果不保留干扰峰。 */
    build_wave(UI_REQUIREMENT_U, 72, 18, 8);
    build_spectrum(UI_REQUIREMENT_U, 10U, 20U, 30U);
    s_profiles[UI_REQUIREMENT_U].wave = (ui_wave_measurement_t){170U, 56U, 120000000U};
    s_profiles[UI_REQUIREMENT_U].spectrum_measurement =
        (ui_spectrum_measurement_t){{120000000U, 240000000U, 360000000U}, {66U, 15U, 7U}};

    for (uint8_t i = 0U; i < 3U; ++i) {
        s_profiles[i].wave_one_period = s_wave_one[i];
        s_profiles[i].wave_three_period = s_wave_three[i];
        s_profiles[i].wave_one_count = DEMO_ONE_POINTS;
        s_profiles[i].wave_three_count = DEMO_THREE_POINTS;
        s_profiles[i].spectrum = s_spectrum[i];
        s_profiles[i].spectrum_count = DEMO_SPECTRUM_POINTS;
    }

    s_initialized = 1U;
}

const ui_demo_profile_t *UI_Demo_Data_GetProfile(ui_requirement_t requirement)
{
    if ((uint32_t)requirement > (uint32_t)UI_REQUIREMENT_U) {
        requirement = UI_REQUIREMENT_UA;
    }
    return &s_profiles[requirement];
}
