/**
 * @file statistics.c
 * @brief 基础统计函数实现。
 */

#include "statistics.h"

#include <math.h>

measurement_status_t statistics_mean_f32(const float *input,
                                         size_t length,
                                         float *mean)
{
    float sum = 0.0f;
    size_t i;

    if ((input == NULL) || (mean == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }
    if (length == 0U) {
        return MEASUREMENT_STATUS_INSUFFICIENT_DATA;
    }

    for (i = 0U; i < length; ++i) {
        sum += input[i];
    }
    *mean = sum / (float)length;
    return MEASUREMENT_STATUS_OK;
}

measurement_status_t statistics_min_max_f32(const float *input,
                                            size_t length,
                                            float *minimum,
                                            float *maximum)
{
    float min_value;
    float max_value;
    size_t i;

    if ((input == NULL) || (minimum == NULL) || (maximum == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }
    if (length == 0U) {
        return MEASUREMENT_STATUS_INSUFFICIENT_DATA;
    }

    min_value = input[0];
    max_value = input[0];
    for (i = 1U; i < length; ++i) {
        if (input[i] < min_value) {
            min_value = input[i];
        }
        if (input[i] > max_value) {
            max_value = input[i];
        }
    }

    *minimum = min_value;
    *maximum = max_value;
    return MEASUREMENT_STATUS_OK;
}

static measurement_status_t calculate_variance(const float *input,
                                               size_t length,
                                               int sample_variance,
                                               float *variance)
{
    float mean;
    float squared_deviation_sum = 0.0f;
    size_t i;
    measurement_status_t status;

    if ((input == NULL) || (variance == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }
    if ((length == 0U) || ((sample_variance != 0) && (length < 2U))) {
        return MEASUREMENT_STATUS_INSUFFICIENT_DATA;
    }

    status = statistics_mean_f32(input, length, &mean);
    if (status != MEASUREMENT_STATUS_OK) {
        return status;
    }

    for (i = 0U; i < length; ++i) {
        const float deviation = input[i] - mean;
        squared_deviation_sum += deviation * deviation;
    }

    *variance = squared_deviation_sum /
                (float)(sample_variance != 0 ? length - 1U : length);
    return MEASUREMENT_STATUS_OK;
}

measurement_status_t statistics_variance_population_f32(const float *input,
                                                        size_t length,
                                                        float *variance)
{
    return calculate_variance(input, length, 0, variance);
}

measurement_status_t statistics_variance_sample_f32(const float *input,
                                                    size_t length,
                                                    float *variance)
{
    return calculate_variance(input, length, 1, variance);
}

measurement_status_t statistics_stddev_population_f32(const float *input,
                                                      size_t length,
                                                      float *standard_deviation)
{
    float variance;
    measurement_status_t status;

    if (standard_deviation == NULL) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }

    status = statistics_variance_population_f32(input, length, &variance);
    if (status != MEASUREMENT_STATUS_OK) {
        return status;
    }
    *standard_deviation = sqrtf(variance);
    return MEASUREMENT_STATUS_OK;
}

measurement_status_t statistics_remove_mean_f32(const float *input,
                                                float *output,
                                                size_t length,
                                                float *removed_mean)
{
    float mean;
    size_t i;
    measurement_status_t status;

    if ((input == NULL) || (output == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }

    status = statistics_mean_f32(input, length, &mean);
    if (status != MEASUREMENT_STATUS_OK) {
        return status;
    }

    for (i = 0U; i < length; ++i) {
        output[i] = input[i] - mean;
    }
    if (removed_mean != NULL) {
        *removed_mean = mean;
    }
    return MEASUREMENT_STATUS_OK;
}

