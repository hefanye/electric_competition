/**
 * @file ad9226_debug.h
 * @brief AD9226 A 通道低速并行采样调试模块。
 *
 * 当前固定硬件映射：
 *   PA8 / TIM1_CH1 -> ACLK
 *   PE0~PE11       <- AD0~AD11（AD0 为最高位）
 *
 * 本模块用于低、中速硬件验证，不用于 65 MSPS 连续采集。
 */

#ifndef AD9226_DEBUG_H
#define AD9226_DEBUG_H

#include "main.h"

HAL_StatusTypeDef AD9226_Debug_Init(TIM_HandleTypeDef *htim,
                                    UART_HandleTypeDef *huart);

/** 在 main 的 HAL_TIM_PeriodElapsedCallback 中、仅 TIM1 时调用。 */
void AD9226_Debug_TimPeriodElapsedCallback(void);

/** 在主循环中调用：完成一帧后通过 USART1 输出统计结果。 */
void AD9226_Debug_Process(void);

#endif /* AD9226_DEBUG_H */
