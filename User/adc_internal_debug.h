/**
 * @file adc_internal_debug.h
 * @brief STM32F407 片内 ADC1 通道 0 (PA0) 采样调试模块。
 *
 * 复用 ioc 已配置的硬件资源：
 *   ADC1 通道 0 (PA0)，12 位，TIM2_TRGO 上升沿触发
 *   TIM2: PSC=83, Period=49 -> 20 kHz 采样率
 *   DMA2_Stream0 循环模式，半字传输
 *
 * 本模块用 DMA NDTR 轮询方式实现双缓冲，不定义 HAL_ADC_ConvCpltCallback /
 * HAL_ADC_ConvHalfCpltCallback，与 adc_baseline_test.c 的回调定义互不冲突。
 *
 * 注意：片内 ADC 量程 0~3.3V 单极性，交流信号必须偏置到 0~3.3V 范围
 * （典型偏置点 1.65V），否则负半周会被钳位。
 */

#ifndef ADC_INTERNAL_DEBUG_H
#define ADC_INTERNAL_DEBUG_H

#include "main.h"

/**
 * @brief 初始化片内 ADC 调试模块。
 * @param hadc  ADC1 句柄
 * @param htim  TIM2 句柄（用于触发 ADC）
 * @param huart USART 句柄（用于打印统计）
 * @retval HAL_OK 成功
 */
HAL_StatusTypeDef ADC_Internal_Debug_Init(ADC_HandleTypeDef *hadc,
                                           TIM_HandleTypeDef *htim,
                                           UART_HandleTypeDef *huart);

/** 在主循环中调用：完成一帧后通过 USART 输出统计结果。 */
void ADC_Internal_Debug_Process(void);

#endif /* ADC_INTERNAL_DEBUG_H */
