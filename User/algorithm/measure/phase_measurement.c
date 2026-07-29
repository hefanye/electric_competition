/**
 * @file phase_measurement.c
 * @brief 单频 I/Q 幅相测量实现。
 */

#include "phase_measurement.h"

#include <math.h>

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

static float window_value(phase_window_t window, size_t index, size_t length)
{
    if (window == PHASE_WINDOW_HANN) {
        return 0.5f - 0.5f *
               cosf(2.0f * MEASUREMENT_PI_F * (float)index /
                    (float)(length - 1U));
    }
    return 1.0f;
}

float phase_wrap_radians(float phase_radians)
{
    const float two_pi = 2.0f * MEASUREMENT_PI_F;

    while (phase_radians >= MEASUREMENT_PI_F) {
        phase_radians -= two_pi;
    }
    while (phase_radians < -MEASUREMENT_PI_F) {
        phase_radians += two_pi;
    }
    return phase_radians;
}

measurement_status_t phase_measure_tone_iq_f32(
    const float *input,
    size_t length,
    const phase_measurement_config_t *config,
    tone_measurement_t *result)
{
    float weighted_sum = 0.0f;
    float weight_sum = 0.0f;
    float in_phase_sum = 0.0f;
    float quadrature_sum = 0.0f;
    float ac_energy_sum = 0.0f;
    float dc_offset;
    size_t i;

    if ((input == NULL) || (config == NULL) || (result == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }

    result->in_phase = 0.0f;
    result->quadrature = 0.0f;
    result->amplitude_peak = 0.0f;
    result->phase_radians = 0.0f;
    result->dc_offset = 0.0f;
    result->quality = 0.0f;
    result->flags = MEASUREMENT_FLAG_NONE;

    if (length < 4U) {
        return MEASUREMENT_STATUS_INSUFFICIENT_DATA;
    }
    if (!(config->sample_rate > 0.0f) ||
        !(config->frequency > 0.0f) ||
        !(config->frequency < 0.5f * config->sample_rate) ||
        !(config->min_amplitude >= 0.0f) ||
        ((config->window != PHASE_WINDOW_RECTANGULAR) &&
         (config->window != PHASE_WINDOW_HANN))) {
        return MEASUREMENT_STATUS_INVALID_PARAM;
    }

    for (i = 0U; i < length; ++i) {
        const float weight = window_value(config->window, i, length);
        weighted_sum += weight * input[i];
        weight_sum += weight;
    }
    if (!(weight_sum > 0.0f)) {
        return MEASUREMENT_STATUS_NUMERIC_ERROR;
    }
    dc_offset = weighted_sum / weight_sum;

    for (i = 0U; i < length; ++i) {
        const float weight = window_value(config->window, i, length);
        const float angle = 2.0f * MEASUREMENT_PI_F * config->frequency *
                            (float)i / config->sample_rate;
        const float ac_value = input[i] - dc_offset;

        in_phase_sum += weight * ac_value * cosf(angle);
        /* 使用负号，使 A*cos(wt+phi) 的返回相位为 +phi。 */
        quadrature_sum -= weight * ac_value * sinf(angle);
        ac_energy_sum += weight * ac_value * ac_value;
    }

    result->in_phase = 2.0f * in_phase_sum / weight_sum;
    result->quadrature = 2.0f * quadrature_sum / weight_sum;
    result->amplitude_peak = sqrtf(result->in_phase * result->in_phase +
                                   result->quadrature * result->quadrature);
    result->phase_radians = phase_wrap_radians(
        atan2f(result->quadrature, result->in_phase));
    result->dc_offset = dc_offset;

    if (!(result->amplitude_peak >= config->min_amplitude) ||
        !(result->amplitude_peak > 0.0f)) {
        result->flags = MEASUREMENT_FLAG_LOW_SIGNAL;
        return MEASUREMENT_STATUS_NO_SIGNAL;
    }

    {
        const float ac_rms = sqrtf(ac_energy_sum / weight_sum);
        const float fundamental_rms = result->amplitude_peak /
                                      MEASUREMENT_SQRT_TWO_F;
        result->quality = ac_rms > 0.0f
                              ? clamp_quality(fundamental_rms / ac_rms)
                              : 0.0f;
    }
    result->flags = MEASUREMENT_FLAG_VALID;
    return MEASUREMENT_STATUS_OK;
}

measurement_status_t phase_measure_difference_iq_f32(
    const float *input_a,
    const float *input_b,
    size_t length,
    const phase_measurement_config_t *config,
    measurement_result_t *result)
{
    tone_measurement_t tone_a;
    tone_measurement_t tone_b;
    measurement_status_t status;

    if ((input_a == NULL) || (input_b == NULL) ||
        (config == NULL) || (result == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }

    result->value = 0.0f;
    result->quality = 0.0f;
    result->flags = MEASUREMENT_FLAG_NONE;
    result->samples_used = 0U;

    status = phase_measure_tone_iq_f32(input_a, length, config, &tone_a);
    if (status != MEASUREMENT_STATUS_OK) {
        result->flags = tone_a.flags;
        return status;
    }
    status = phase_measure_tone_iq_f32(input_b, length, config, &tone_b);
    if (status != MEASUREMENT_STATUS_OK) {
        result->flags = tone_b.flags;
        return status;
    }

    result->value = phase_wrap_radians(tone_b.phase_radians -
                                       tone_a.phase_radians);
    result->quality = tone_a.quality < tone_b.quality
                          ? tone_a.quality
                          : tone_b.quality;
    result->flags = MEASUREMENT_FLAG_VALID;
    result->samples_used = length;
    return MEASUREMENT_STATUS_OK;
}
