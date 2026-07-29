/**
 * @file ad9226_parallel.h
 * @brief Reusable low-to-medium-speed parallel capture driver for AD9226.
 *
 * This driver is intended for STM32F4 timer-interrupt based bring-up and
 * instrument prototypes. It is not intended to reach the AD9226 65 MSPS
 * maximum rate; use FPGA or a dedicated parallel capture solution for that.
 */

#ifndef AD9226_PARALLEL_H
#define AD9226_PARALLEL_H

#include "main.h"

typedef struct
{
    uint16_t min_code;
    uint16_t max_code;
    uint16_t peak_to_peak;
    uint32_t mean_code;
} AD9226_FrameStats;

typedef struct
{
    TIM_HandleTypeDef *timer;
    uint32_t pwm_channel;
    GPIO_TypeDef *data_port;
    uint8_t data_lsb_pin;
    uint16_t *frame_buffer;
    uint16_t frame_length;
    uint8_t ad0_is_msb;
    volatile uint16_t sample_count;
    volatile uint8_t frame_ready;
} AD9226_Handle;

HAL_StatusTypeDef AD9226_Init(AD9226_Handle *handle,
                              TIM_HandleTypeDef *timer,
                              uint32_t pwm_channel,
                              GPIO_TypeDef *data_port,
                              uint8_t data_lsb_pin,
                              uint8_t ad0_is_msb,
                              uint16_t *frame_buffer,
                              uint16_t frame_length);

HAL_StatusTypeDef AD9226_Start(AD9226_Handle *handle);
HAL_StatusTypeDef AD9226_Stop(AD9226_Handle *handle);

/** Call this from HAL_TIM_PeriodElapsedCallback(). */
void AD9226_OnTimerUpdate(AD9226_Handle *handle,
                          TIM_HandleTypeDef *callback_timer);

uint8_t AD9226_IsFrameReady(const AD9226_Handle *handle);
void AD9226_ReleaseFrame(AD9226_Handle *handle);

HAL_StatusTypeDef AD9226_ComputeFrameStats(const uint16_t *samples,
                                           uint16_t count,
                                           AD9226_FrameStats *stats);

#endif /* AD9226_PARALLEL_H */
