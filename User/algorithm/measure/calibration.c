/**
 * @file calibration.c
 * @brief 测量值校准算法实现。
 */

#include "calibration.h"

#include <math.h>

static measurement_status_t interpolate_segment(float raw_1,
                                                float reference_1,
                                                float raw_2,
                                                float reference_2,
                                                float raw_value,
                                                float *corrected_value)
{
    const float raw_span = raw_2 - raw_1;

    if (!(raw_span > 0.0f)) {
        return MEASUREMENT_STATUS_INVALID_PARAM;
    }
    *corrected_value = reference_1 +
                       (raw_value - raw_1) *
                           (reference_2 - reference_1) / raw_span;
    return MEASUREMENT_STATUS_OK;
}

measurement_status_t calibration_linear_from_two_points(
    float raw_1,
    float reference_1,
    float raw_2,
    float reference_2,
    linear_calibration_t *calibration)
{
    if (calibration == NULL) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }
    if (raw_2 == raw_1) {
        return MEASUREMENT_STATUS_INVALID_PARAM;
    }

    calibration->gain = (reference_2 - reference_1) / (raw_2 - raw_1);
    calibration->offset = reference_1 - calibration->gain * raw_1;
    calibration->rmse = 0.0f;
    return MEASUREMENT_STATUS_OK;
}

measurement_status_t calibration_linear_fit_f32(
    const float *raw_values,
    const float *reference_values,
    size_t length,
    linear_calibration_t *calibration)
{
    float raw_mean = 0.0f;
    float reference_mean = 0.0f;
    float covariance_sum = 0.0f;
    float raw_variance_sum = 0.0f;
    float squared_error_sum = 0.0f;
    size_t i;

    if ((raw_values == NULL) || (reference_values == NULL) ||
        (calibration == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }
    if (length < 2U) {
        return MEASUREMENT_STATUS_INSUFFICIENT_DATA;
    }

    for (i = 0U; i < length; ++i) {
        raw_mean += raw_values[i];
        reference_mean += reference_values[i];
    }
    raw_mean /= (float)length;
    reference_mean /= (float)length;

    for (i = 0U; i < length; ++i) {
        const float raw_deviation = raw_values[i] - raw_mean;
        covariance_sum += raw_deviation *
                          (reference_values[i] - reference_mean);
        raw_variance_sum += raw_deviation * raw_deviation;
    }
    if (!(raw_variance_sum > 0.0f)) {
        return MEASUREMENT_STATUS_INVALID_PARAM;
    }

    calibration->gain = covariance_sum / raw_variance_sum;
    calibration->offset = reference_mean - calibration->gain * raw_mean;

    for (i = 0U; i < length; ++i) {
        const float estimated = calibration->gain * raw_values[i] +
                                calibration->offset;
        const float error = reference_values[i] - estimated;
        squared_error_sum += error * error;
    }
    calibration->rmse = sqrtf(squared_error_sum / (float)length);
    return MEASUREMENT_STATUS_OK;
}

measurement_status_t calibration_linear_apply(
    const linear_calibration_t *calibration,
    float raw_value,
    float *corrected_value)
{
    if ((calibration == NULL) || (corrected_value == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }
    *corrected_value = calibration->gain * raw_value + calibration->offset;
    return MEASUREMENT_STATUS_OK;
}

measurement_status_t calibration_piecewise_apply_f32(
    const float *raw_points,
    const float *reference_points,
    size_t point_count,
    float raw_value,
    bool allow_extrapolation,
    float *corrected_value)
{
    size_t i;

    if ((raw_points == NULL) || (reference_points == NULL) ||
        (corrected_value == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }
    if (point_count < 2U) {
        return MEASUREMENT_STATUS_INSUFFICIENT_DATA;
    }

    for (i = 1U; i < point_count; ++i) {
        if (!(raw_points[i] > raw_points[i - 1U])) {
            return MEASUREMENT_STATUS_INVALID_PARAM;
        }
    }

    if (raw_value <= raw_points[0]) {
        if (!allow_extrapolation) {
            *corrected_value = reference_points[0];
            return MEASUREMENT_STATUS_OK;
        }
        return interpolate_segment(raw_points[0], reference_points[0],
                                   raw_points[1], reference_points[1],
                                   raw_value, corrected_value);
    }

    if (raw_value >= raw_points[point_count - 1U]) {
        if (!allow_extrapolation) {
            *corrected_value = reference_points[point_count - 1U];
            return MEASUREMENT_STATUS_OK;
        }
        return interpolate_segment(raw_points[point_count - 2U],
                                   reference_points[point_count - 2U],
                                   raw_points[point_count - 1U],
                                   reference_points[point_count - 1U],
                                   raw_value, corrected_value);
    }

    for (i = 1U; i < point_count; ++i) {
        if (raw_value <= raw_points[i]) {
            return interpolate_segment(raw_points[i - 1U],
                                       reference_points[i - 1U],
                                       raw_points[i], reference_points[i],
                                       raw_value, corrected_value);
        }
    }

    return MEASUREMENT_STATUS_NUMERIC_ERROR;
}

