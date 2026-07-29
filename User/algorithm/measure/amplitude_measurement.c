/**
 * @file amplitude_measurement.c
 * @brief 幅值测量函数实现。
 */

#include "amplitude_measurement.h"

#include <math.h>

#include "statistics.h"

measurement_status_t amplitude_rms_f32(const float *input,
                                       size_t length,
                                       float *rms)
{
    float square_sum = 0.0f;
    size_t i;

    if ((input == NULL) || (rms == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }
    if (length == 0U) {
        return MEASUREMENT_STATUS_INSUFFICIENT_DATA;
    }

    for (i = 0U; i < length; ++i) {
        square_sum += input[i] * input[i];
    }
    *rms = sqrtf(square_sum / (float)length);
    return MEASUREMENT_STATUS_OK;
}

measurement_status_t amplitude_ac_rms_f32(const float *input,
                                          size_t length,
                                          float *ac_rms,
                                          float *dc_offset)
{
    float mean;
    float square_sum = 0.0f;
    size_t i;
    measurement_status_t status;

    if ((input == NULL) || (ac_rms == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }

    status = statistics_mean_f32(input, length, &mean);
    if (status != MEASUREMENT_STATUS_OK) {
        return status;
    }

    for (i = 0U; i < length; ++i) {
        const float ac_value = input[i] - mean;
        square_sum += ac_value * ac_value;
    }
    *ac_rms = sqrtf(square_sum / (float)length);
    if (dc_offset != NULL) {
        *dc_offset = mean;
    }
    return MEASUREMENT_STATUS_OK;
}

measurement_status_t amplitude_peak_to_peak_f32(const float *input,
                                                size_t length,
                                                float *peak_to_peak)
{
    float minimum;
    float maximum;
    measurement_status_t status;

    if (peak_to_peak == NULL) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }

    status = statistics_min_max_f32(input, length, &minimum, &maximum);
    if (status != MEASUREMENT_STATUS_OK) {
        return status;
    }
    *peak_to_peak = maximum - minimum;
    return MEASUREMENT_STATUS_OK;
}

measurement_status_t amplitude_absolute_peak_f32(const float *input,
                                                 size_t length,
                                                 float *absolute_peak)
{
    float peak = 0.0f;
    size_t i;

    if ((input == NULL) || (absolute_peak == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }
    if (length == 0U) {
        return MEASUREMENT_STATUS_INSUFFICIENT_DATA;
    }

    for (i = 0U; i < length; ++i) {
        const float magnitude = fabsf(input[i]);
        if (magnitude > peak) {
            peak = magnitude;
        }
    }
    *absolute_peak = peak;
    return MEASUREMENT_STATUS_OK;
}

measurement_status_t amplitude_crest_factor_f32(const float *input,
                                                size_t length,
                                                float *crest_factor)
{
    float rms;
    float peak;
    measurement_status_t status;

    if (crest_factor == NULL) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }

    status = amplitude_rms_f32(input, length, &rms);
    if (status != MEASUREMENT_STATUS_OK) {
        return status;
    }
    status = amplitude_absolute_peak_f32(input, length, &peak);
    if (status != MEASUREMENT_STATUS_OK) {
        return status;
    }
    if (!(rms > 0.0f)) {
        return MEASUREMENT_STATUS_NO_SIGNAL;
    }

    *crest_factor = peak / rms;
    return MEASUREMENT_STATUS_OK;
}

measurement_status_t amplitude_sine_peak_from_ac_rms(float ac_rms,
                                                     float *peak_amplitude)
{
    if (peak_amplitude == NULL) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }
    if (ac_rms < 0.0f) {
        return MEASUREMENT_STATUS_INVALID_PARAM;
    }

    *peak_amplitude = MEASUREMENT_SQRT_TWO_F * ac_rms;
    return MEASUREMENT_STATUS_OK;
}
