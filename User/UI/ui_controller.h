#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#include "ui_types.h"

void UI_Controller_Init(void);
void UI_Controller_Process(void);
uint8_t UI_Controller_HandleToken(const char *token);
void UI_Controller_SetRequirement(ui_requirement_t requirement);
void UI_Controller_SetWaveMeasurement(const ui_wave_measurement_t *measurement);
void UI_Controller_SetSpectrumMeasurement(const ui_spectrum_measurement_t *measurement);
void UI_Controller_SetWaveSamples(const uint8_t *samples, uint16_t count);
void UI_Controller_SetWaveSamplesForView(ui_view_t view, const uint8_t *samples, uint16_t count);
void UI_Controller_SetSpectrumSamples(const uint8_t *samples, uint16_t count);
void UI_Controller_RefreshWave(ui_view_t view);
void UI_Controller_RefreshSpectrum(void);

#endif
