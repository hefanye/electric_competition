/**
 * @file amplitude_measurement.h
 * @brief 波形幅值、有效值和波峰因数测量。
 */

#ifndef AMPLITUDE_MEASUREMENT_H
#define AMPLITUDE_MEASUREMENT_H

#include "measurement_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 计算包含直流分量的总 RMS。 */
measurement_status_t amplitude_rms_f32(const float *input,
                                       size_t length,
                                       float *rms);

/** @brief 去除均值后计算交流 RMS，同时可返回直流均值。 */
measurement_status_t amplitude_ac_rms_f32(const float *input,
                                          size_t length,
                                          float *ac_rms,
                                          float *dc_offset);

/** @brief 计算峰峰值 maximum - minimum。 */
measurement_status_t amplitude_peak_to_peak_f32(const float *input,
                                                size_t length,
                                                float *peak_to_peak);

/** @brief 计算绝对峰值 max(abs(x[n]))。 */
measurement_status_t amplitude_absolute_peak_f32(const float *input,
                                                 size_t length,
                                                 float *absolute_peak);

/**
 * @brief 计算波峰因数 absolute_peak / rms。
 * @return 全零信号返回 MEASUREMENT_STATUS_NO_SIGNAL。
 */
measurement_status_t amplitude_crest_factor_f32(const float *input,
                                                size_t length,
                                                float *crest_factor);

/** @brief 仅在确认波形是正弦波时，将 AC RMS 换算为正弦峰值。 */
measurement_status_t amplitude_sine_peak_from_ac_rms(float ac_rms,
                                                     float *peak_amplitude);

#ifdef __cplusplus
}
#endif

#endif /* AMPLITUDE_MEASUREMENT_H */

