/**
 * @file signal_quality.c
 * @brief 信号有效性和削顶分析实现。
 */

#include "signal_quality.h"

#include <float.h>
#include <math.h>

static int sample_is_finite(float value)
{
    /* NaN 不等于自身；正负无穷的绝对值大于 FLT_MAX。 */
    return (value == value) && (value <= FLT_MAX) && (value >= -FLT_MAX);
}

static float clamp_unit(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

measurement_status_t signal_quality_analyze_f32(
    const float *input,
    size_t length,
    const signal_quality_config_t *config,
    signal_quality_result_t *result)
{
    float sum = 0.0f;
    float square_sum = 0.0f;
    float minimum = 0.0f;
    float maximum = 0.0f;
    const float low_clip_threshold = config != NULL
                                         ? config->input_low_limit +
                                               config->clipping_margin
                                         : 0.0f;
    const float high_clip_threshold = config != NULL
                                          ? config->input_high_limit -
                                                config->clipping_margin
                                          : 0.0f;
    size_t valid_count = 0U;
    size_t i;

    if ((input == NULL) || (config == NULL) || (result == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }

    result->minimum = 0.0f;
    result->maximum = 0.0f;
    result->mean = 0.0f;
    result->ac_rms = 0.0f;
    result->clipped_fraction = 0.0f;
    result->quality = 0.0f;
    result->clipped_count = 0U;
    result->invalid_count = 0U;
    result->flags = MEASUREMENT_FLAG_NONE;
    result->usable = false;

    if (length == 0U) {
        return MEASUREMENT_STATUS_INSUFFICIENT_DATA;
    }
    if (!(config->input_high_limit > config->input_low_limit) ||
        !(config->clipping_margin >= 0.0f) ||
        !(config->clipping_margin <
          0.5f * (config->input_high_limit - config->input_low_limit)) ||
        !(config->min_ac_rms >= 0.0f) ||
        !(config->max_clipped_fraction >= 0.0f) ||
        !(config->max_clipped_fraction <= 1.0f)) {
        return MEASUREMENT_STATUS_INVALID_PARAM;
    }

    for (i = 0U; i < length; ++i) {
        const float sample = input[i];

        if (!sample_is_finite(sample)) {
            result->invalid_count++;
            continue;
        }

        if (valid_count == 0U) {
            minimum = sample;
            maximum = sample;
        } else {
            if (sample < minimum) {
                minimum = sample;
            }
            if (sample > maximum) {
                maximum = sample;
            }
        }

        if ((sample <= low_clip_threshold) ||
            (sample >= high_clip_threshold)) {
            result->clipped_count++;
        }

        sum += sample;
        square_sum += sample * sample;
        valid_count++;
    }

    if (valid_count == 0U) {
        result->flags = MEASUREMENT_FLAG_INVALID_SAMPLE;
        return MEASUREMENT_STATUS_NUMERIC_ERROR;
    }

    result->minimum = minimum;
    result->maximum = maximum;
    result->mean = sum / (float)valid_count;
    {
        float ac_variance = square_sum / (float)valid_count -
                            result->mean * result->mean;
        if (ac_variance < 0.0f) {
            ac_variance = 0.0f;
        }
        result->ac_rms = sqrtf(ac_variance);
    }
    result->clipped_fraction = (float)result->clipped_count /
                               (float)valid_count;
    result->quality = clamp_unit(1.0f - result->clipped_fraction);

    if (result->invalid_count > 0U) {
        result->flags |= MEASUREMENT_FLAG_INVALID_SAMPLE;
        result->quality *= (float)valid_count / (float)length;
    }
    if (result->clipped_fraction > config->max_clipped_fraction) {
        result->flags |= MEASUREMENT_FLAG_CLIPPED;
    }
    if (result->ac_rms < config->min_ac_rms) {
        result->flags |= MEASUREMENT_FLAG_LOW_SIGNAL;
    }

    result->usable = (result->invalid_count == 0U) &&
                     (result->clipped_fraction <= config->max_clipped_fraction) &&
                     (result->ac_rms >= config->min_ac_rms);
    if (result->usable) {
        result->flags |= MEASUREMENT_FLAG_VALID;
    }

    return MEASUREMENT_STATUS_OK;
}

