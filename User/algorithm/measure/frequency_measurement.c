/**
 * @file frequency_measurement.c
 * @brief 迟滞过零测频实现。
 */

#include "frequency_measurement.h"

#include <math.h>
#include <stdbool.h>

static float clamp_quality(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

measurement_status_t frequency_measure_zero_crossing_f32(
    const float *input,
    size_t length,
    const frequency_zero_crossing_config_t *config,
    measurement_result_t *result)
{
    bool armed = false;
    bool candidate_pending = false;
    bool have_previous_crossing = false;
    float candidate_crossing = 0.0f;
    float previous_crossing = 0.0f;
    float period_mean = 0.0f;
    float period_m2 = 0.0f;
    float min_period;
    float max_period;
    size_t detected_crossings = 0U;
    size_t valid_periods = 0U;
    size_t i;

    if ((input == NULL) || (config == NULL) || (result == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }

    result->value = 0.0f;
    result->quality = 0.0f;
    result->flags = MEASUREMENT_FLAG_NONE;
    result->samples_used = 0U;

    if (length < 3U) {
        return MEASUREMENT_STATUS_INSUFFICIENT_DATA;
    }
    if (!(config->sample_rate > 0.0f) ||
        !(config->negative_threshold < 0.0f) ||
        !(config->positive_threshold > 0.0f) ||
        !(config->min_frequency >= 0.0f) ||
        !(config->max_frequency > config->min_frequency) ||
        !(config->max_frequency < 0.5f * config->sample_rate)) {
        return MEASUREMENT_STATUS_INVALID_PARAM;
    }

    min_period = config->sample_rate / config->max_frequency;
    max_period = config->min_frequency > 0.0f
                     ? config->sample_rate / config->min_frequency
                     : (float)length;

    if (input[0] <= config->negative_threshold) {
        armed = true;
    }

    for (i = 1U; i < length; ++i) {
        const float previous_sample = input[i - 1U];
        const float current_sample = input[i];

        if (!armed && (current_sample <= config->negative_threshold)) {
            armed = true;
            candidate_pending = false;
        }

        if (armed && !candidate_pending &&
            (previous_sample <= 0.0f) && (current_sample > 0.0f)) {
            const float denominator = current_sample - previous_sample;
            if (denominator > 0.0f) {
                candidate_crossing = (float)(i - 1U) -
                                     previous_sample / denominator;
                candidate_pending = true;
            }
        }

        if (armed && candidate_pending &&
            (current_sample >= config->positive_threshold)) {
            detected_crossings++;

            if (!have_previous_crossing) {
                previous_crossing = candidate_crossing;
                have_previous_crossing = true;
            } else {
                const float period = candidate_crossing - previous_crossing;

                if ((period >= min_period) && (period <= max_period)) {
                    const float delta = period - period_mean;
                    valid_periods++;
                    period_mean += delta / (float)valid_periods;
                    period_m2 += delta * (period - period_mean);
                    previous_crossing = candidate_crossing;
                } else if (period > max_period) {
                    /* 间隔过长时从当前过零点重新开始，便于丢信号后恢复。 */
                    previous_crossing = candidate_crossing;
                }
            }

            armed = false;
            candidate_pending = false;
        }
    }

    if (detected_crossings == 0U) {
        result->flags = MEASUREMENT_FLAG_LOW_SIGNAL;
        return MEASUREMENT_STATUS_NO_SIGNAL;
    }
    if (valid_periods == 0U) {
        result->samples_used = detected_crossings;
        return MEASUREMENT_STATUS_INSUFFICIENT_DATA;
    }
    if (!(period_mean > 0.0f)) {
        return MEASUREMENT_STATUS_NUMERIC_ERROR;
    }

    result->value = config->sample_rate / period_mean;
    result->samples_used = valid_periods;
    result->flags = MEASUREMENT_FLAG_VALID;

    if (valid_periods > 1U) {
        const float period_variance = period_m2 / (float)(valid_periods - 1U);
        const float coefficient_of_variation = sqrtf(period_variance) / period_mean;
        result->quality = clamp_quality(1.0f - coefficient_of_variation);
    } else {
        result->quality = 0.5f;
    }

    return MEASUREMENT_STATUS_OK;
}

