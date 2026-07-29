/**
 * @file frequency_measurement.h
 * @brief 基于迟滞过零和线性插值的频率测量。
 *
 * 适合单频、信噪比较高的周期信号。输入应当已经去除明显直流偏置。
 */

#ifndef FREQUENCY_MEASUREMENT_H
#define FREQUENCY_MEASUREMENT_H

#include "measurement_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 迟滞过零测频配置。 */
typedef struct {
    float sample_rate;        /**< 采样率，单位 Hz。 */
    float negative_threshold; /**< 重新使能检测所需的负门限，必须小于 0。 */
    float positive_threshold; /**< 确认上升沿所需的正门限，必须大于 0。 */
    float min_frequency;      /**< 允许的最低频率；0 表示不限制最长周期。 */
    float max_frequency;      /**< 允许的最高频率，必须小于采样率的一半。 */
} frequency_zero_crossing_config_t;

/**
 * @brief 使用上升过零点测量频率。
 *
 * 算法先要求信号低于 negative_threshold，再寻找由负到正的零点，并等待
 * 信号超过 positive_threshold 后确认该次过零。零点位置使用线性插值，最后
 * 对多个有效周期取平均。
 *
 * @param result value 为 Hz，quality 越接近 1 表示周期一致性越好，
 *               samples_used 表示参与平均的有效周期数。
 */
measurement_status_t frequency_measure_zero_crossing_f32(
    const float *input,
    size_t length,
    const frequency_zero_crossing_config_t *config,
    measurement_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* FREQUENCY_MEASUREMENT_H */

