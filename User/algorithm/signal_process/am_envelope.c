/**
 * @file am_envelope.c
 * @brief 不依赖 Hilbert 变换的轻量级 AM 包络检波器。
 */

#include "am_envelope.h"

#include <math.h>

signal_process_status_t am_envelope_init_alpha_f32(
    am_envelope_follower_t *follower,
    float attack_alpha,
    float release_alpha)
{
    if (follower == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (attack_alpha <= 0.0f || attack_alpha > 1.0f ||
        release_alpha <= 0.0f || release_alpha > 1.0f) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    follower->attack_alpha = attack_alpha;
    follower->release_alpha = release_alpha;
    follower->envelope = 0.0f;
    follower->initialized = 0;
    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t am_envelope_init_times_f32(
    am_envelope_follower_t *follower,
    float sample_rate_hz,
    float attack_time_seconds,
    float release_time_seconds)
{
    float attack_alpha;
    float release_alpha;

    if (follower == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (sample_rate_hz <= 0.0f || attack_time_seconds < 0.0f ||
        release_time_seconds < 0.0f) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    attack_alpha = attack_time_seconds == 0.0f
        ? 1.0f
        : 1.0f - expf(-1.0f / (sample_rate_hz * attack_time_seconds));
    release_alpha = release_time_seconds == 0.0f
        ? 1.0f
        : 1.0f - expf(-1.0f / (sample_rate_hz * release_time_seconds));
    return am_envelope_init_alpha_f32(
        follower, attack_alpha, release_alpha);
}

signal_process_status_t am_envelope_reset_f32(
    am_envelope_follower_t *follower,
    float initial_envelope)
{
    if (follower == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (initial_envelope < 0.0f) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    follower->envelope = initial_envelope;
    follower->initialized = 1;
    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t am_envelope_process_sample_f32(
    am_envelope_follower_t *follower,
    float input,
    float *envelope)
{
    float rectified;
    float alpha;

    if (follower == NULL || envelope == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (follower->attack_alpha <= 0.0f || follower->attack_alpha > 1.0f ||
        follower->release_alpha <= 0.0f || follower->release_alpha > 1.0f) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    rectified = fabsf(input);
    if (!follower->initialized) {
        follower->envelope = rectified;
        follower->initialized = 1;
    } else {
        alpha = rectified > follower->envelope
            ? follower->attack_alpha
            : follower->release_alpha;
        follower->envelope += alpha * (rectified - follower->envelope);
    }

    *envelope = follower->envelope;
    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t am_envelope_process_block_f32(
    am_envelope_follower_t *follower,
    const float *input,
    float *output,
    size_t length)
{
    size_t i;

    if (follower == NULL || input == NULL || output == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (length == 0U) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    for (i = 0U; i < length; ++i) {
        signal_process_status_t status = am_envelope_process_sample_f32(
            follower, input[i], &output[i]);
        if (status != SIGNAL_PROCESS_STATUS_OK) {
            return status;
        }
    }
    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t am_modulation_index_f32(
    const float *envelope,
    size_t length,
    float minimum_sum,
    float *modulation_index,
    float *minimum_envelope,
    float *maximum_envelope)
{
    size_t i;
    float minimum;
    float maximum;

    if (envelope == NULL || modulation_index == NULL ||
        minimum_envelope == NULL || maximum_envelope == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (length == 0U || minimum_sum < 0.0f) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    minimum = envelope[0];
    maximum = envelope[0];
    for (i = 1U; i < length; ++i) {
        if (envelope[i] < minimum) {
            minimum = envelope[i];
        }
        if (envelope[i] > maximum) {
            maximum = envelope[i];
        }
    }

    if (minimum < 0.0f) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }
    *minimum_envelope = minimum;
    *maximum_envelope = maximum;
    if (maximum + minimum <= minimum_sum) {
        *modulation_index = 0.0f;
        return SIGNAL_PROCESS_STATUS_NO_SIGNAL;
    }

    *modulation_index = (maximum - minimum) / (maximum + minimum);
    return SIGNAL_PROCESS_STATUS_OK;
}
