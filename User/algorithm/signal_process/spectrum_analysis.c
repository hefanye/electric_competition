/**
 * @file spectrum_analysis.c
 * @brief 带窗增益校正的单边幅度谱和峰值插值。
 */

#include "spectrum_analysis.h"

#include <math.h>

signal_process_status_t spectrum_compute_real_f32(
    const float *input,
    size_t length,
    sp_window_type_t window,
    sp_complex_f32_t *fft_buffer,
    float *magnitudes,
    size_t magnitude_capacity)
{
    size_t i;
    const size_t required_magnitudes = length / 2U + 1U;
    float coherent_gain;
    signal_process_status_t status;

    if (input == NULL || fft_buffer == NULL || magnitudes == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (!fft_is_power_of_two(length) ||
        magnitude_capacity < required_magnitudes) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    status = sp_window_coherent_gain_f32(window, length, &coherent_gain);
    if (status != SIGNAL_PROCESS_STATUS_OK) {
        return status;
    }

    for (i = 0U; i < length; ++i) {
        float coefficient;
        status = sp_window_coefficient_f32(window, i, length, &coefficient);
        if (status != SIGNAL_PROCESS_STATUS_OK) {
            return status;
        }
        fft_buffer[i].real = input[i] * coefficient;
        fft_buffer[i].imag = 0.0f;
    }

    status = fft_transform_inplace_f32(fft_buffer, length, 0);
    if (status != SIGNAL_PROCESS_STATUS_OK) {
        return status;
    }

    for (i = 0U; i < required_magnitudes; ++i) {
        float amplitude = hypotf(fft_buffer[i].real, fft_buffer[i].imag)
                        / ((float)length * coherent_gain);
        /* 实信号负频率与正频率成对，DC 与 Nyquist 点除外。 */
        if (i != 0U && i != length / 2U) {
            amplitude *= 2.0f;
        }
        magnitudes[i] = amplitude;
    }

    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t spectrum_find_peak_f32(
    const float *magnitudes,
    size_t magnitude_count,
    float sample_rate_hz,
    size_t fft_length,
    size_t minimum_bin,
    size_t maximum_bin,
    spectrum_peak_t *result)
{
    size_t i;
    size_t peak_bin;
    float delta = 0.0f;
    float interpolated_amplitude;

    if (magnitudes == NULL || result == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (magnitude_count == 0U || sample_rate_hz <= 0.0f ||
        fft_length == 0U || minimum_bin > maximum_bin ||
        maximum_bin >= magnitude_count) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    peak_bin = minimum_bin;
    for (i = minimum_bin + 1U; i <= maximum_bin; ++i) {
        if (magnitudes[i] > magnitudes[peak_bin]) {
            peak_bin = i;
        }
    }

    interpolated_amplitude = magnitudes[peak_bin];
    if (peak_bin > 0U && peak_bin + 1U < magnitude_count) {
        const float left = magnitudes[peak_bin - 1U];
        const float center = magnitudes[peak_bin];
        const float right = magnitudes[peak_bin + 1U];
        const float denominator = left - 2.0f * center + right;

        if (fabsf(denominator) > 1.0e-20f) {
            delta = 0.5f * (left - right) / denominator;
            if (delta < -0.5f) {
                delta = -0.5f;
            } else if (delta > 0.5f) {
                delta = 0.5f;
            }
            interpolated_amplitude = center
                                   - 0.25f * (left - right) * delta;
        }
    }

    result->peak_bin = peak_bin;
    result->interpolated_bin = (float)peak_bin + delta;
    result->frequency_hz = result->interpolated_bin * sample_rate_hz
                         / (float)fft_length;
    result->amplitude_peak = interpolated_amplitude;
    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t spectrum_amplitude_to_db_f32(
    float amplitude,
    float reference,
    float floor_db,
    float *decibels)
{
    float floor_amplitude;

    if (decibels == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (amplitude < 0.0f || reference <= 0.0f || floor_db > 0.0f) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    floor_amplitude = reference * powf(10.0f, floor_db / 20.0f);
    if (amplitude < floor_amplitude) {
        amplitude = floor_amplitude;
    }
    *decibels = 20.0f * log10f(amplitude / reference);
    return SIGNAL_PROCESS_STATUS_OK;
}
