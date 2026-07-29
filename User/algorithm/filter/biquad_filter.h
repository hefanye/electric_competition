/**
 * @file biquad_filter.h
 * @brief 通用二阶 IIR（Biquad）滤波器及常用系数设计函数。
 *
 * 一个运行核心支持低通、高通、带通和陷波。所有系数均在初始化时计算，
 * 逐样本处理阶段只执行乘加，适合嵌入式实时使用。
 */

#ifndef BIQUAD_FILTER_H
#define BIQUAD_FILTER_H

#include "filter_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 归一化后的 Biquad 系数，a0 已归一化为 1。 */
typedef struct {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
} biquad_coefficients_t;

/** Direct Form I 的 Biquad 状态。 */
typedef struct {
    biquad_coefficients_t coefficients;
    float x1;
    float x2;
    float y1;
    float y2;
} biquad_filter_t;

/** @brief 使用已归一化的系数初始化滤波器。 */
filter_status_t biquad_filter_init(biquad_filter_t *filter,
                                   const biquad_coefficients_t *coefficients);

/** @brief 清除输入输出历史，保留当前系数。 */
filter_status_t biquad_filter_reset(biquad_filter_t *filter);

/** @brief 运行期间替换系数，并可选择是否清除历史状态。 */
filter_status_t biquad_filter_set_coefficients(
    biquad_filter_t *filter,
    const biquad_coefficients_t *coefficients,
    int reset_state);

/** @brief 处理单个样本。 */
filter_status_t biquad_filter_process_sample(biquad_filter_t *filter,
                                             float input,
                                             float *output);

/** @brief 连续处理数据块，input 与 output 可以相同。 */
filter_status_t biquad_filter_process_block(biquad_filter_t *filter,
                                            const float *input,
                                            float *output,
                                            size_t length);

/** @brief 设计二阶低通，Q 必须大于 0。 */
filter_status_t biquad_design_lowpass(biquad_coefficients_t *coefficients,
                                      float sample_rate,
                                      float cutoff_frequency,
                                      float q);

/** @brief 设计二阶高通，Q 必须大于 0。 */
filter_status_t biquad_design_highpass(biquad_coefficients_t *coefficients,
                                       float sample_rate,
                                       float cutoff_frequency,
                                       float q);

/**
 * @brief 设计恒定峰值增益的二阶带通滤波器。
 * @param center_frequency 中心频率。
 * @param q 品质因数，越大带宽越窄。
 */
filter_status_t biquad_design_bandpass(biquad_coefficients_t *coefficients,
                                       float sample_rate,
                                       float center_frequency,
                                       float q);

/** @brief 设计二阶陷波器，适合 50/60 Hz 等窄带干扰抑制。 */
filter_status_t biquad_design_notch(biquad_coefficients_t *coefficients,
                                    float sample_rate,
                                    float notch_frequency,
                                    float q);

/** @brief 设计二阶 Butterworth 低通，内部使用 Q = 1/sqrt(2)。 */
filter_status_t biquad_design_butterworth_lowpass(
    biquad_coefficients_t *coefficients,
    float sample_rate,
    float cutoff_frequency);

/** @brief 设计二阶 Butterworth 高通，内部使用 Q = 1/sqrt(2)。 */
filter_status_t biquad_design_butterworth_highpass(
    biquad_coefficients_t *coefficients,
    float sample_rate,
    float cutoff_frequency);

#ifdef __cplusplus
}
#endif

#endif /* BIQUAD_FILTER_H */

