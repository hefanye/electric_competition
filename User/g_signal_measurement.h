#ifndef G_SIGNAL_MEASUREMENT_H
#define G_SIGNAL_MEASUREMENT_H

#include "ui_types.h"
#include "stm32f4xx_hal.h"
#include "fft.h"  /* sp_complex_f32_t（共享 FFT 缓冲类型） */

/* G 题基础测量链：AD9226(A) -> GPIOE[11:0] -> TIM1_UP DMA -> 8192 点 FFT。 */
#define G_SIGNAL_SAMPLE_RATE_HZ      4000000UL
#define G_SIGNAL_FRAME_SAMPLES       8192U
#define G_SIGNAL_INPUT_GAIN          1.0f

/* 第三题抗干扰模式：10.5MHz 采样，Nyquist=5.25MHz，1MHz+ 干扰清晰不混叠。
 * TIM1 时钟 168MHz，ARR=15 → 168M/16=10.5MHz。 */
#define G_SIGNAL_SAMPLE_RATE_10M_HZ  10500000UL
#define G_SIGNAL_TIM1_ARR_4M         41U
#define G_SIGNAL_TIM1_ARR_10M        15U

/* 采样参数（共享） */
#define G_SIGNAL_DISCARD_SAMPLES     8U
#define G_SIGNAL_DMA_SAMPLES         (G_SIGNAL_FRAME_SAMPLES + G_SIGNAL_DISCARD_SAMPLES)
#define G_SIGNAL_BIN_COUNT           (G_SIGNAL_FRAME_SAMPLES / 2U + 1U)

/* 拟合参数（共享） */
#define FIT_MAX_HARMONICS            3U
#define FIT_MATRIX_DIM               (2U * FIT_MAX_HARMONICS + 1U)  /* 7 */

HAL_StatusTypeDef GSignal_Init(TIM_HandleTypeDef *htim);
void GSignal_Request(ui_requirement_t requirement, ui_view_t view);
void GSignal_Process(void);
uint8_t GSignal_IsBusy(void);
void GSignal_DMA2_Stream5_IRQHandler(void);

/* 第三题模式切换：动态修改 TIM1 ARR 切换采样率。
 * 4MHz 用于第一二题（Ua/Ub），10.5MHz 用于第三题（U，抗单频干扰）。
 * 返回切换后的实际采样率（Hz），便于上位层校验。 */
uint32_t GSignal_SetSampleRate(uint8_t use_10m);
uint32_t GSignal_GetSampleRate(void);

/* ===== 共享底层函数（供 g_signal_u.c 第三题模块复用）===== */
/* FFT/幅度谱缓冲区：两模块互斥共享（不会同时采集）。
 * U 模块用前 G_SIGNALU_FRAME_SAMPLES 个元素做 2048 点 FFT。
 * 导出仅为节省 SRAM，不构成逻辑耦合。 */
extern sp_complex_f32_t s_fft[G_SIGNAL_FRAME_SAMPLES];
extern float s_magnitude[G_SIGNAL_BIN_COUNT];

/* AD9226 数据位重映射：GPIOE->IDR 散布位 → 连续12位码值 */
uint16_t remap_ad9226(uint16_t idr);
/* ADC 码值转输入电压（V），含前端增益补偿 */
float code_to_input_voltage(uint16_t code);
/* 高斯消元法解线性方程组 A·x = b，带部分主元选择 */
uint8_t solve_linear_system(float *A, float *b, float *x, uint32_t n);
/* 频率精化（IEEE 1057 3参数高斯-牛顿迭代） */
float refine_frequency(const uint16_t *raw, uint32_t length,
                       float sample_rate, float freq_init);
/* 多正弦最小二乘拟合：返回基波峰峰值（mV） */
float fit_multisine_and_upp(const uint16_t *raw, uint32_t length,
                            float sample_rate,
                            const float freq_in[3],
                            uint32_t harmonics_count,
                            float *fund_freq_out,
                            float harmonics_freq[3],
                            float harmonics_amp[3],
                            float beta_out[FIT_MATRIX_DIM]);
/* 从幅度谱找真峰，按幅度降序输出。返回真峰个数（1~3）。
 * 注意：操作 g_signal_measurement.c 的 s_magnitude 数组，仅在一二题流程中可用。 */
uint32_t find_top3_peaks(float freq_out[3], float amp_out[3]);

#endif
