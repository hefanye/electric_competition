/**
 * @file dc_blocker.h
 * @brief 流式 DC 阻断（一阶高通）滤波器。
 *
 * 差分方程：y[n] = R * (y[n-1] + x[n] - x[n-1])。
 * 适合连续数据去除 ADC 偏置。对于一次性 FFT 数据块，直接减去块均值通常更简单。
 */

#ifndef DC_BLOCKER_H
#define DC_BLOCKER_H

#include <stdbool.h>

#include "filter_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** DC 阻断器状态。 */
typedef struct {
    float pole;             /**< 极点系数 R，范围 (0, 1)。 */
    float previous_input;   /**< 上一次输入。 */
    float previous_output;  /**< 上一次输出。 */
    bool initialized;       /**< 是否已经接收过样本。 */
} dc_blocker_t;

/** @brief 直接使用极点系数初始化，pole 范围为 (0, 1)。 */
filter_status_t dc_blocker_init_pole(dc_blocker_t *filter, float pole);

/**
 * @brief 根据采样率和高通截止频率初始化。
 *
 * pole = exp(-2*pi*cutoff/sample_rate)。截止频率应远低于待测信号频率。
 */
filter_status_t dc_blocker_init_cutoff(dc_blocker_t *filter,
                                       float sample_rate,
                                       float cutoff_frequency);

/** @brief 清除历史状态。复位后的第一个样本输出为 0。 */
filter_status_t dc_blocker_reset(dc_blocker_t *filter);

/** @brief 处理单个样本。 */
filter_status_t dc_blocker_process_sample(dc_blocker_t *filter,
                                          float input,
                                          float *output);

/** @brief 连续处理数据块，input 与 output 可以相同。 */
filter_status_t dc_blocker_process_block(dc_blocker_t *filter,
                                         const float *input,
                                         float *output,
                                         size_t length);

#ifdef __cplusplus
}
#endif

#endif /* DC_BLOCKER_H */

