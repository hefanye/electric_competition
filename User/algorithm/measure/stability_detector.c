/**
 * @file stability_detector.c
 * @brief 流式稳定性检测实现。
 */

#include "stability_detector.h"

#include <float.h>
#include <math.h>

static int value_is_finite(float value)
{
    return (value == value) && (value <= FLT_MAX) && (value >= -FLT_MAX);
}

measurement_status_t stability_detector_init(stability_detector_t *detector,
                                             float *work_buffer,
                                             size_t window_size,
                                             float absolute_tolerance,
                                             float relative_tolerance)
{
    size_t i;

    if ((detector == NULL) || (work_buffer == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }
    if ((window_size < 2U) || !(absolute_tolerance >= 0.0f) ||
        !(relative_tolerance >= 0.0f)) {
        return MEASUREMENT_STATUS_INVALID_PARAM;
    }

    detector->buffer = work_buffer;
    detector->window_size = window_size;
    detector->index = 0U;
    detector->count = 0U;
    detector->absolute_tolerance = absolute_tolerance;
    detector->relative_tolerance = relative_tolerance;

    for (i = 0U; i < window_size; ++i) {
        work_buffer[i] = 0.0f;
    }
    return MEASUREMENT_STATUS_OK;
}

measurement_status_t stability_detector_reset(stability_detector_t *detector)
{
    size_t i;

    if ((detector == NULL) || (detector->buffer == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }
    if (detector->window_size < 2U) {
        return MEASUREMENT_STATUS_INVALID_PARAM;
    }

    for (i = 0U; i < detector->window_size; ++i) {
        detector->buffer[i] = 0.0f;
    }
    detector->index = 0U;
    detector->count = 0U;
    return MEASUREMENT_STATUS_OK;
}

measurement_status_t stability_detector_process(stability_detector_t *detector,
                                                float value,
                                                stability_result_t *result)
{
    float sum = 0.0f;
    float square_deviation_sum = 0.0f;
    float minimum;
    float maximum;
    float relative_span;
    size_t i;

    if ((detector == NULL) || (detector->buffer == NULL) || (result == NULL)) {
        return MEASUREMENT_STATUS_NULL_POINTER;
    }
    if ((detector->window_size < 2U) || !value_is_finite(value)) {
        return MEASUREMENT_STATUS_INVALID_PARAM;
    }

    detector->buffer[detector->index] = value;
    detector->index++;
    if (detector->index >= detector->window_size) {
        detector->index = 0U;
    }
    if (detector->count < detector->window_size) {
        detector->count++;
    }

    minimum = detector->buffer[0];
    maximum = detector->buffer[0];
    for (i = 0U; i < detector->count; ++i) {
        const float sample = detector->buffer[i];
        sum += sample;
        if (sample < minimum) {
            minimum = sample;
        }
        if (sample > maximum) {
            maximum = sample;
        }
    }

    result->mean = sum / (float)detector->count;
    for (i = 0U; i < detector->count; ++i) {
        const float deviation = detector->buffer[i] - result->mean;
        square_deviation_sum += deviation * deviation;
    }

    result->minimum = minimum;
    result->maximum = maximum;
    result->standard_deviation = sqrtf(square_deviation_sum /
                                       (float)detector->count);
    result->actual_span = maximum - minimum;
    relative_span = detector->relative_tolerance * fabsf(result->mean);
    result->allowed_span = relative_span > detector->absolute_tolerance
                               ? relative_span
                               : detector->absolute_tolerance;
    result->sample_count = detector->count;
    result->stable = (detector->count == detector->window_size) &&
                     (result->actual_span <= result->allowed_span);

    if (detector->count < detector->window_size) {
        result->quality = (float)detector->count /
                          (float)detector->window_size;
    } else if (result->allowed_span > 0.0f) {
        const float ratio = result->actual_span / result->allowed_span;
        result->quality = ratio >= 1.0f ? 0.0f : 1.0f - ratio;
    } else {
        result->quality = result->actual_span == 0.0f ? 1.0f : 0.0f;
    }

    return MEASUREMENT_STATUS_OK;
}

