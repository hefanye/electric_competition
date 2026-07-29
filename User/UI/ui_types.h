#ifndef UI_TYPES_H
#define UI_TYPES_H

#include <stdint.h>

typedef enum { UI_REQUIREMENT_UA = 0, UI_REQUIREMENT_UB, UI_REQUIREMENT_U } ui_requirement_t;
typedef enum { UI_VIEW_NONE = 0, UI_VIEW_WAVE_1PERIOD, UI_VIEW_WAVE_3PERIOD, UI_VIEW_SPECTRUM } ui_view_t;

typedef struct {
    uint32_t upp_mV;
    uint32_t urms_mV;
    uint32_t fundamental_mHz;
} ui_wave_measurement_t;

typedef struct {
    uint32_t frequency_mHz[3];
    uint32_t amplitude_mV[3];
} ui_spectrum_measurement_t;

#endif
