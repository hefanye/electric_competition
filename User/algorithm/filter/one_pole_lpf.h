/**
 * @file one_pole_lpf.h
 * @brief 一阶低通滤波器。
 *
 * 差分方程：y[n] = y[n-1] + alpha * (x[n] - y[n-1])。
 * 适合对缓慢变化量进行低成本平滑。
 */

#ifndef ONE_POLE_LPF_H
#define ONE_POLE_LPF_H

#include <stdbool.h>

#include "filter_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 一阶低通状态。 */
typedef struct {
    float alpha;       /**< 平滑系数，范围 (0, 1]。 */
    float previous;    /**< 上一次输出。 */
    bool initialized;  /**< 是否已有有效初始状态。 */
} one_pole_lpf_t;

/** @brief 直接使用 alpha 初始化，alpha 范围为 (0, 1]。 */
filter_status_t one_pole_lpf_init_alpha(one_pole_lpf_t *filter, float alpha);

/**
 * @brief 根据采样率和截止频率初始化。
 * @param sample_rate 采样率，必须大于 0。
 * @param cutoff_frequency 截止频率，范围为 (0, sample_rate/2)。
 */
filter_status_t one_pole_lpf_init_cutoff(one_pole_lpf_t *filter,
                                         float sample_rate,
                                         float cutoff_frequency);

/**
 * @brief 复位并指定初始输出。
 *
 * 若不调用本函数，初始化后的第一个输入样本会直接成为第一个输出，
 * 从而避免由零初值造成的启动跳变。
 */
filter_status_t one_pole_lpf_reset(one_pole_lpf_t *filter, float initial_output);

/** @brief 处理单个样本。 */
filter_status_t one_pole_lpf_process_sample(one_pole_lpf_t *filter,
                                            float input,
                                            float *output);

/** @brief 连续处理数据块，input 与 output 可以相同。 */
filter_status_t one_pole_lpf_process_block(one_pole_lpf_t *filter,
                                           const float *input,
                                           float *output,
                                           size_t length);

#ifdef __cplusplus
}
#endif

#endif /* ONE_POLE_LPF_H */

