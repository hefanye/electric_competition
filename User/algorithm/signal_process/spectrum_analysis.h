/**
 * @file spectrum_analysis.h
 * @brief 实信号单边幅度谱、频谱峰值搜索和抛物线插值。
 */

#ifndef SPECTRUM_ANALYSIS_H
#define SPECTRUM_ANALYSIS_H

#include <stddef.h>
#include "fft.h"
#include "window_functions.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t peak_bin;          /**< 最大幅值所在的整数频点。 */
    float interpolated_bin;   /**< 抛物线插值得到的小数频点。 */
    float frequency_hz;       /**< 插值后的频率。 */
    float amplitude_peak;     /**< 单边峰值幅度估计。 */
} spectrum_peak_t;

/**
 * 计算实信号的单边峰值幅度谱。
 * fft_buffer 至少 length 个复数；magnitudes 至少 length/2+1 个 float。
 */
signal_process_status_t spectrum_compute_real_f32(
    const float *input,
    size_t length,
    sp_window_type_t window,
    sp_complex_f32_t *fft_buffer,
    float *magnitudes,
    size_t magnitude_capacity);

/**
 * 在闭区间 [minimum_bin, maximum_bin] 搜索峰值并作三点抛物线插值。
 * magnitude_count 通常为 FFT 长度 / 2 + 1。
 */
signal_process_status_t spectrum_find_peak_f32(
    const float *magnitudes,
    size_t magnitude_count,
    float sample_rate_hz,
    size_t fft_length,
    size_t minimum_bin,
    size_t maximum_bin,
    spectrum_peak_t *result);

/** 把线性幅值转换为 dB；reference 必须大于 0。 */
signal_process_status_t spectrum_amplitude_to_db_f32(
    float amplitude,
    float reference,
    float floor_db,
    float *decibels);

#ifdef __cplusplus
}
#endif

#endif
