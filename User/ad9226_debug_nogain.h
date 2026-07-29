/**
 * @file ad9226_debug_nogain.h
 * @brief AD9226 A 通道高速并行采样调试模块（去增益版本，DMA 方式）。
 *
 * TIM1_CH1 (PA8) 输出 ACLK，TIM1 Update 事件触发 DMA2_Stream1
 * (Channel6 = TIM1_UP) 自动从 GPIOE->IDR 搬运 256 点到 SRAM，
 * DMA 完成后主循环统计并通过 USART1 输出。
 *
 * 当前固定硬件映射：
 *   PA8 / TIM1_CH1 -> ACLK
 *   PE0~PE11       <- AD0~AD11（AD0 为最高位）
 */

#ifndef AD9226_DEBUG_NOGAIN_H
#define AD9226_DEBUG_NOGAIN_H

#include "main.h"

HAL_StatusTypeDef AD9226_Debug_Init(TIM_HandleTypeDef *htim,
                                    UART_HandleTypeDef *huart);

/** DMA 模式下保留为空，仅兼容 main.c 的 HAL_TIM_PeriodElapsedCallback。 */
void AD9226_Debug_TimPeriodElapsedCallback(void);

/** 在主循环中调用：完成一帧后通过 USART1 输出统计结果。 */
void AD9226_Debug_Process(void);

/** DMA 中断处理函数（由 stm32f4xx_it.c 的 DMA2_Stream5_IRQHandler 调用）。 */
void AD9226_DMA_IRQHandler(void);

#endif /* AD9226_DEBUG_NOGAIN_H */
