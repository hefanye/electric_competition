/**
 * @file g_signal_pc_debug.h
 * @brief G题测量结果 PC 串口旁路输出（仅输出，不控制采集）。
 *
 * 在 analyse_frame() 完成后调用 GSignal_PCDebug_Print()，将 Upp/Urms/f1、
 * Top3 频谱、200 点波形和 200 点频谱通过 USART1 输出到 PC 串口助手。
 * 采集仍由串口屏令牌触发（比赛模式），本模块仅做旁路监听输出。
 */

#ifndef G_SIGNAL_PC_DEBUG_H
#define G_SIGNAL_PC_DEBUG_H

#include "ui_types.h"
#include "stm32f4xx_hal.h"

void GSignal_PCDebug_Init(UART_HandleTypeDef *huart);

void GSignal_PCDebug_Print(const ui_wave_measurement_t *wave,
                           const ui_spectrum_measurement_t *spectrum,
                           const uint8_t *wave_samples,
                           const uint8_t *spectrum_samples);

#endif /* G_SIGNAL_PC_DEBUG_H */
