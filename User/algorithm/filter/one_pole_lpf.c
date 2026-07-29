/**
 * @file one_pole_lpf.c
 * @brief 一阶低通滤波器实现。
 */

#include "one_pole_lpf.h"

#include <math.h>

filter_status_t one_pole_lpf_init_alpha(one_pole_lpf_t *filter, float alpha)
{
    if (filter == NULL) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if (!(alpha > 0.0f && alpha <= 1.0f)) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    filter->alpha = alpha;
    filter->previous = 0.0f;
    filter->initialized = false;
    return FILTER_STATUS_OK;
}

filter_status_t one_pole_lpf_init_cutoff(one_pole_lpf_t *filter,
                                         float sample_rate,
                                         float cutoff_frequency)
{
    float alpha;

    if (filter == NULL) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if (!(sample_rate > 0.0f) || !(cutoff_frequency > 0.0f) ||
        !(cutoff_frequency < 0.5f * sample_rate)) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    alpha = 1.0f - expf(-2.0f * FILTER_PI_F * cutoff_frequency / sample_rate);
    return one_pole_lpf_init_alpha(filter, alpha);
}

filter_status_t one_pole_lpf_reset(one_pole_lpf_t *filter, float initial_output)
{
    if (filter == NULL) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if (!(filter->alpha > 0.0f && filter->alpha <= 1.0f)) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    filter->previous = initial_output;
    filter->initialized = true;
    return FILTER_STATUS_OK;
}

filter_status_t one_pole_lpf_process_sample(one_pole_lpf_t *filter,
                                            float input,
                                            float *output)
{
    if ((filter == NULL) || (output == NULL)) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if (!(filter->alpha > 0.0f && filter->alpha <= 1.0f)) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    if (!filter->initialized) {
        filter->previous = input;
        filter->initialized = true;
    } else {
        filter->previous += filter->alpha * (input - filter->previous);
    }

    *output = filter->previous;
    return FILTER_STATUS_OK;
}

filter_status_t one_pole_lpf_process_block(one_pole_lpf_t *filter,
                                           const float *input,
                                           float *output,
                                           size_t length)
{
    size_t i;
    filter_status_t status;

    if ((filter == NULL) || (input == NULL) || (output == NULL)) {
        return FILTER_STATUS_NULL_POINTER;
    }

    for (i = 0U; i < length; ++i) {
        status = one_pole_lpf_process_sample(filter, input[i], &output[i]);
        if (status != FILTER_STATUS_OK) {
            return status;
        }
    }

    return FILTER_STATUS_OK;
}

