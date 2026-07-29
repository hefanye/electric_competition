/**
 * @file phase_measurement.h
 * @brief 已知频率下的单频 I/Q 幅值、相位和双通道相位差测量。
 */

#ifndef PHASE_MEASUREMENT_H
#define PHASE_MEASUREMENT_H

#include "measurement_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** I/Q 相关时使用的窗函数。 */
typedef enum {
    PHASE_WINDOW_RECTANGULAR = 0,
    PHASE_WINDOW_HANN = 1
} phase_window_t;

/** 单频 I/Q 分析配置。 */
typedef struct {
    float sample_rate;    /**< 采样率，单位 Hz。 */
    float frequency;      /**< 已知待测频率，单位 Hz。 */
    float min_amplitude;  /**< 小于该峰值时判为无信号。 */
    phase_window_t window;/**< 矩形窗或 Hann 窗。 */
} phase_measurement_config_t;

/** 单频 I/Q 分析结果。 */
typedef struct {
    float in_phase;       /**< 同相相关分量 I。 */
    float quadrature;     /**< 正交相关分量 Q。 */
    float amplitude_peak; /**< 被测单频的峰值幅度。 */
    float phase_radians;  /**< 相位，范围 [-pi, pi)。 */
    float dc_offset;      /**< 输入信号的加权均值。 */
    float quality;        /**< 基波 RMS 占总 AC RMS 的比例，范围 0～1。 */
    uint32_t flags;       /**< measurement_flag_t 标志。 */
} tone_measurement_t;

/** @brief 测量单路信号在已知频率处的幅值和相位。 */
measurement_status_t phase_measure_tone_iq_f32(
    const float *input,
    size_t length,
    const phase_measurement_config_t *config,
    tone_measurement_t *result);

/**
 * @brief 测量两个同步采样通道在已知频率处的相位差。
 * @param result value 为 phase_b - phase_a，单位弧度，范围 [-pi, pi)。
 */
measurement_status_t phase_measure_difference_iq_f32(
    const float *input_a,
    const float *input_b,
    size_t length,
    const phase_measurement_config_t *config,
    measurement_result_t *result);

/** @brief 将任意弧度角归一化到 [-pi, pi)。 */
float phase_wrap_radians(float phase_radians);

#ifdef __cplusplus
}
#endif

#endif /* PHASE_MEASUREMENT_H */

