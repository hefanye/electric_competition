#ifndef G_SIGNAL_MEASUREMENT_H
#define G_SIGNAL_MEASUREMENT_H

#include "ui_types.h"
#include "stm32f4xx_hal.h"

/* G 题基础测量链：AD9226(A) -> GPIOE[11:0] -> TIM1_UP DMA -> 8192 点 FFT。 */
#define G_SIGNAL_SAMPLE_RATE_HZ      4000000UL
#define G_SIGNAL_FRAME_SAMPLES       8192U
#define G_SIGNAL_INPUT_GAIN          1.0f

HAL_StatusTypeDef GSignal_Init(TIM_HandleTypeDef *htim);
void GSignal_Request(ui_requirement_t requirement, ui_view_t view);
void GSignal_Process(void);
uint8_t GSignal_IsBusy(void);
void GSignal_DMA2_Stream5_IRQHandler(void);

#endif
