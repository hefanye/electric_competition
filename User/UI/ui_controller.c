/**
 * @file ui_controller.c
 * @brief G题显示流程控制器。
 *
 * 串口屏只上报简短令牌，例如 UI:PAGE:WAVE1；本文件根据当前题目选择，
 * 决定写入哪个波形控件、哪些数值和频谱。后续接入真实算法时，只需调用
 * UI_Controller_SetWaveMeasurement / SetSpectrumMeasurement / SetWaveSamples。
 */
#include "ui_controller.h"

#include "ui_demo_data.h"
#include "ui_display.h"
#include "ui_hmi_map.h"
#include "ui_plot.h"
#include "g_signal_measurement.h"
#include "g_signal_u.h"

#include <string.h>

static ui_requirement_t s_requirement;
static ui_wave_measurement_t s_wave_override;
static ui_spectrum_measurement_t s_spectrum_override;
static const uint8_t *s_wave_override_samples;
static uint16_t s_wave_override_count;
static const uint8_t *s_wave_one_override_samples;
static uint16_t s_wave_one_override_count;
static const uint8_t *s_wave_three_override_samples;
static uint16_t s_wave_three_override_count;
static const uint8_t *s_spectrum_override_samples;
static uint16_t s_spectrum_override_count;
static uint8_t s_has_wave_override;
static uint8_t s_has_spectrum_override;

static void show_spectrum_page(void);

static const ui_wave_measurement_t *current_wave_measurement(const ui_demo_profile_t *profile)
{
    return (s_has_wave_override != 0U) ? &s_wave_override : &profile->wave;
}

static const ui_spectrum_measurement_t *current_spectrum_measurement(const ui_demo_profile_t *profile)
{
    return (s_has_spectrum_override != 0U) ? &s_spectrum_override :
                                              &profile->spectrum_measurement;
}

void UI_Controller_Init(void)
{
    UI_Demo_Data_Init();
    s_requirement = UI_REQUIREMENT_UA;
    s_wave_override_samples = NULL;
    s_wave_override_count = 0U;
    s_spectrum_override_samples = NULL;
    s_spectrum_override_count = 0U;
    s_wave_one_override_samples = NULL;
    s_wave_one_override_count = 0U;
    s_wave_three_override_samples = NULL;
    s_wave_three_override_count = 0U;
    s_has_wave_override = 0U;
    s_has_spectrum_override = 0U;
}

void UI_Controller_Process(void)
{
    UI_Plot_Process();
    /* 两个测量模块都轮询，各自通过 busy 标志互斥运行 */
    GSignal_Process();
    GSignalU_Process();
}

void UI_Controller_SetRequirement(ui_requirement_t requirement)
{
    if ((uint32_t)requirement <= (uint32_t)UI_REQUIREMENT_U) {
        s_requirement = requirement;
    }
}

void UI_Controller_SetWaveMeasurement(const ui_wave_measurement_t *measurement)
{
    if (measurement != NULL) {
        s_wave_override = *measurement;
        s_has_wave_override = 1U;
    }
}

void UI_Controller_SetSpectrumMeasurement(const ui_spectrum_measurement_t *measurement)
{
    if (measurement != NULL) {
        s_spectrum_override = *measurement;
        s_has_spectrum_override = 1U;
    }
}

void UI_Controller_SetWaveSamples(const uint8_t *samples, uint16_t count)
{
    s_wave_override_samples = samples;
    s_wave_override_count = count;
}

void UI_Controller_SetWaveSamplesForView(ui_view_t view, const uint8_t *samples, uint16_t count)
{
    if (view == UI_VIEW_WAVE_3PERIOD) {
        s_wave_three_override_samples = samples;
        s_wave_three_override_count = count;
    } else {
        s_wave_one_override_samples = samples;
        s_wave_one_override_count = count;
    }
}

void UI_Controller_SetSpectrumSamples(const uint8_t *samples, uint16_t count)
{
    s_spectrum_override_samples = samples;
    s_spectrum_override_count = count;
}

static void show_wave_page(ui_view_t view)
{
    const ui_demo_profile_t *profile = UI_Demo_Data_GetProfile(s_requirement);
    const uint8_t *samples;
    uint16_t count;
    uint8_t component_id;

    UI_Display_ShowWave(view, current_wave_measurement(profile));

    if (view == UI_VIEW_WAVE_3PERIOD && s_wave_three_override_samples != NULL && s_wave_three_override_count > 0U) {
        samples = s_wave_three_override_samples;
        count = s_wave_three_override_count;
    } else if (view == UI_VIEW_WAVE_1PERIOD && s_wave_one_override_samples != NULL && s_wave_one_override_count > 0U) {
        samples = s_wave_one_override_samples;
        count = s_wave_one_override_count;
    } else if (s_wave_override_samples != NULL && s_wave_override_count > 0U) {
        samples = s_wave_override_samples;
        count = s_wave_override_count;
    } else if (view == UI_VIEW_WAVE_3PERIOD) {
        samples = profile->wave_three_period;
        count = profile->wave_three_count;
    } else {
        samples = profile->wave_one_period;
        count = profile->wave_one_count;
    }

    component_id = (view == UI_VIEW_WAVE_3PERIOD) ?
                   UI_HMI_WAVE3_COMPONENT_ID : UI_HMI_WAVE1_COMPONENT_ID;
    UI_Plot_Start(component_id, UI_HMI_WAVE_CHANNEL, samples, count);
}

void UI_Controller_RefreshWave(ui_view_t view) { show_wave_page(view); }
void UI_Controller_RefreshSpectrum(void) { show_spectrum_page(); }

static void show_spectrum_page(void)
{
    const ui_demo_profile_t *profile = UI_Demo_Data_GetProfile(s_requirement);

    UI_Display_ShowSpectrum(current_spectrum_measurement(profile));
    if (s_spectrum_override_samples != NULL && s_spectrum_override_count > 0U) {
        UI_Plot_Start(UI_HMI_SPECTRUM_COMPONENT_ID, UI_HMI_SPECTRUM_CHANNEL,
                      s_spectrum_override_samples, s_spectrum_override_count);
    } else {
        UI_Plot_Start(UI_HMI_SPECTRUM_COMPONENT_ID, UI_HMI_SPECTRUM_CHANNEL,
                      profile->spectrum, profile->spectrum_count);
    }
}

uint8_t UI_Controller_HandleToken(const char *token)
{
    if (token == NULL) {
        return 0U;
    }

    if (strcmp(token, "UI:REQ:UA") == 0) {
        UI_Controller_SetRequirement(UI_REQUIREMENT_UA);
        /* 切回一二题：先中止 U 模块（如果在运行），再切换采样率 */
        GSignalU_Abort();
        GSignal_SetSampleRate(0U);
        return 1U;
    }
    if (strcmp(token, "UI:REQ:UB") == 0) {
        UI_Controller_SetRequirement(UI_REQUIREMENT_UB);
        GSignalU_Abort();
        GSignal_SetSampleRate(0U);
        return 1U;
    }
    if (strcmp(token, "UI:REQ:U") == 0) {
        UI_Controller_SetRequirement(UI_REQUIREMENT_U);
        /* 第三题抗干扰：GSignal_SetSampleRate(1) 会停止一二题 DMA 并切换 ARR。
         * U 模块的采集由后续 WAVE/SPECTRUM 令牌触发 GSignalU_Request。 */
        GSignal_SetSampleRate(1U);
        return 1U;
    }
    if (strcmp(token, "UI:PAGE:HOME") == 0) {
        UI_Plot_Stop();
        UI_Display_SetStatus("READY");
        return 1U;
    }
    if (strcmp(token, "UI:PAGE:MENU") == 0) {
        /* p_menu owns t_menu_req and t_menu_cfg, so write them only after
         * that page reports that it has completed its own initialization. */
        uint32_t rate;
        UI_Display_ShowRequirement(s_requirement);
        /* 校验采样率配置是否切换成功，显示到 t_menu_cfg 控件 */
        rate = GSignal_GetSampleRate();
        if (s_requirement == UI_REQUIREMENT_U) {
            UI_Display_SetMenuConfig((rate == G_SIGNAL_SAMPLE_RATE_10M_HZ) ?
                                     "10M OK" : "10M FAIL");
        } else {
            UI_Display_SetMenuConfig((rate == G_SIGNAL_SAMPLE_RATE_HZ) ?
                                     "4M OK" : "4M FAIL");
        }
        return 1U;
    }
    if (strcmp(token, "UI:PAGE:WAVE_SELECT") == 0) {
        /* This page owns t_ws_req, so refresh it only after the page is
         * active.  s_requirement remains selected from the home-page key. */
        UI_Display_ShowRequirementAt(UI_HMI_WAVE_SELECT_REQUIREMENT_TEXT,
                                     s_requirement);
        return 1U;
    }
    if (strcmp(token, "UI:PAGE:WAVE1") == 0) {
        /* U 模式走独立模块，一二题走原模块 */
        if (s_requirement == UI_REQUIREMENT_U) {
            GSignalU_Request(s_requirement, UI_VIEW_WAVE_1PERIOD);
        } else {
            GSignal_Request(s_requirement, UI_VIEW_WAVE_1PERIOD);
        }
        return 1U;
    }
    if (strcmp(token, "UI:PAGE:WAVE3") == 0) {
        if (s_requirement == UI_REQUIREMENT_U) {
            GSignalU_Request(s_requirement, UI_VIEW_WAVE_3PERIOD);
        } else {
            GSignal_Request(s_requirement, UI_VIEW_WAVE_3PERIOD);
        }
        return 1U;
    }
    if (strcmp(token, "UI:PAGE:SPECTRUM") == 0) {
        if (s_requirement == UI_REQUIREMENT_U) {
            GSignalU_Request(s_requirement, UI_VIEW_SPECTRUM);
        } else {
            GSignal_Request(s_requirement, UI_VIEW_SPECTRUM);
        }
        return 1U;
    }

    return 0U;
}
