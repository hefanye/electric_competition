/**
 * @file ui_display.h
 * @brief 波形页、频谱页与主页面状态文本的显示封装。
 */
#ifndef UI_DISPLAY_H
#define UI_DISPLAY_H

#include "ui_types.h"

void UI_Display_SetStatus(const char *text);
void UI_Display_ShowRequirement(ui_requirement_t requirement);
void UI_Display_ShowRequirementAt(const char *control, ui_requirement_t requirement);
void UI_Display_ShowWave(ui_view_t view, const ui_wave_measurement_t *measurement);
void UI_Display_ShowSpectrum(const ui_spectrum_measurement_t *measurement);

#endif
