/**
 * @file ui_display.c
 * @brief 所有串口屏文本控件名称集中由 ui_hmi_map.h 管理。
 */
#include "ui_display.h"

#include "screen_protocol.h"
#include "ui_hmi_map.h"

#include <stdio.h>

static void send_u32_text(const char *control, uint32_t value, const char *suffix)
{
    char text[32];
    (void)snprintf(text, sizeof(text), "%lu%s", (unsigned long)value, suffix);
    Screen_SendText(control, text);
}

void UI_Display_SetStatus(const char *text)
{
    Screen_SendText(UI_HMI_STATUS_TEXT, text);
}

void UI_Display_ShowRequirementAt(const char *control, ui_requirement_t requirement)
{
    /* TJC dynamic text uses its configured byte encoding.  Keep this status
     * ASCII-only so it is dependable regardless of whether the HMI font is
     * GBK or UTF-8. Static labels on the page may still be Chinese. */
    const char *text = "REQ: Ua";

    if (requirement == UI_REQUIREMENT_UB) {
        text = "REQ: Ub";
    } else if (requirement == UI_REQUIREMENT_U) {
        text = "REQ: U";
    }

    if (control != NULL) {
        Screen_SendText(control, text);
    }
}

void UI_Display_ShowRequirement(ui_requirement_t requirement)
{
    UI_Display_ShowRequirementAt(UI_HMI_MENU_REQUIREMENT_TEXT, requirement);
}

void UI_Display_ShowWave(ui_view_t view, const ui_wave_measurement_t *measurement)
{
    const char *upp_control;
    const char *urms_control;
    const char *frequency_control;
    uint32_t frequency_hz;

    if (measurement == NULL) {
        return;
    }

    if (view == UI_VIEW_WAVE_3PERIOD) {
        upp_control = UI_HMI_WAVE3_UPP_TEXT;
        urms_control = UI_HMI_WAVE3_URMS_TEXT;
        frequency_control = UI_HMI_WAVE3_FREQ_TEXT;
    } else {
        upp_control = UI_HMI_WAVE1_UPP_TEXT;
        urms_control = UI_HMI_WAVE1_URMS_TEXT;
        frequency_control = UI_HMI_WAVE1_FREQ_TEXT;
    }

    send_u32_text(upp_control, measurement->upp_mV, " mV");
    send_u32_text(urms_control, measurement->urms_mV, " mV");
    frequency_hz = measurement->fundamental_mHz / 1000U;
    send_u32_text(frequency_control, frequency_hz, " Hz");
}

void UI_Display_ShowSpectrum(const ui_spectrum_measurement_t *measurement)
{
    static const char *const controls[3] =
    {
        UI_HMI_SPECTRUM_1_TEXT,
        UI_HMI_SPECTRUM_2_TEXT,
        UI_HMI_SPECTRUM_3_TEXT
    };
    uint8_t i;

    if (measurement == NULL) {
        return;
    }

    for (i = 0U; i < 3U; ++i) {
        char text[40];
        uint32_t frequency_khz_x10 = measurement->frequency_mHz[i] / 100000U;
        (void)snprintf(text, sizeof(text), "f%u=%lu.%lu kHz  A=%lu mV",
                       (unsigned int)(i + 1U),
                       (unsigned long)(frequency_khz_x10 / 10U),
                       (unsigned long)(frequency_khz_x10 % 10U),
                       (unsigned long)measurement->amplitude_mV[i]);
        Screen_SendText(controls[i], text);
    }
}
