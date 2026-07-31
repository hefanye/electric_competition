/**
 * @file g_signal_u.h
 * @brief 第三题抗干扰模式（U 模式）：10.5MHz 采样 + 频域陷波。
 *
 * 与 g_signal_measurement.c（第一二题）完全解耦：
 *   - 独立的 DMA 缓冲、FFT 缓冲、状态变量
 *   - 独立的中断处理函数
 *   - 仅复用 g_signal_measurement.h 导出的底层算法函数
 *    （remap_ad9226 / code_to_input_voltage / refine_frequency /
 *      fit_multisine_and_upp / solve_linear_system）
 *
 * 抗干扰策略：
 *   1. 10.5MHz 采样，Nyquist=5.25MHz，>=1MHz 干扰清晰不混叠
 *   2. FFT 后将 >=800kHz 的频率分量置零（频域陷波），频谱显示不含干扰
 *   3. Upp/Urms 由多正弦最小二乘拟合得出，拟合基函数与干扰频率正交，
 *      干扰能量不污染拟合结果，无需 IFFT 回时域
 *   4. 波形由拟合参数数学重建，纯净无干扰
 */
#ifndef G_SIGNAL_U_H
#define G_SIGNAL_U_H

#include "ui_types.h"
#include "stm32f4xx_hal.h"

/* U 模式采样参数（独立于一二题的 8192 点）
 * 2048 点 FFT：10.5MHz 采样下 bin=5.13kHz，50kHz 在 bin9，500kHz 在 bin97
 * 内存占用约 25KB，与一二题 97KB 合计约 122KB < 128KB 主 SRAM */
#define G_SIGNALU_FRAME_SAMPLES    2048U
#define G_SIGNALU_BIN_COUNT        (G_SIGNALU_FRAME_SAMPLES / 2U + 1U)
/* 陷波门限：>=800kHz 的频率分量置零（题目信号<=500kHz，干扰>=1MHz） */
#define G_SIGNALU_NOTCH_BIN        156U   /* 800kHz / 5.13kHz */

HAL_StatusTypeDef GSignalU_Init(TIM_HandleTypeDef *htim);

/* 启动一次 U 模式采集。由 UI_Controller 在 UI:REQ:U 模式下
 * 收到 WAVE1/WAVE3/SPECTRUM 令牌时调用。 */
void GSignalU_Request(ui_requirement_t requirement, ui_view_t view);

/* 在主循环中调用，处理采集完成后的分析。 */
void GSignalU_Process(void);

uint8_t GSignalU_IsBusy(void);

/* DMA2_Stream5 完成中断处理，由 stm32f4xx_it.c 根据当前模式分发调用。 */
void GSignalU_DMA2_Stream5_IRQHandler(void);

/* 强制中止 U 模式采集（模式切换时调用，确保 DMA 停止、busy 清零）。 */
void GSignalU_Abort(void);

#endif /* G_SIGNAL_U_H */
