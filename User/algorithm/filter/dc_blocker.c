/**
 * @file dc_blocker.c
 * @brief 流式 DC 阻断滤波器实现。
 */

#include "dc_blocker.h"

#include <math.h>

filter_status_t dc_blocker_init_pole(dc_blocker_t *filter, float pole)
{
    if (filter == NULL) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if (!(pole > 0.0f && pole < 1.0f)) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    filter->pole = pole;
    filter->previous_input = 0.0f;
    filter->previous_output = 0.0f;
    filter->initialized = false;
    return FILTER_STATUS_OK;
}

filter_status_t dc_blocker_init_cutoff(dc_blocker_t *filter,
                                       float sample_rate,
                                       float cutoff_frequency)
{
    float pole;

    if (filter == NULL) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if (!(sample_rate > 0.0f) || !(cutoff_frequency > 0.0f) ||
        !(cutoff_frequency < 0.5f * sample_rate)) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    pole = expf(-2.0f * FILTER_PI_F * cutoff_frequency / sample_rate);
    return dc_blocker_init_pole(filter, pole);
}

filter_status_t dc_blocker_reset(dc_blocker_t *filter)
{
    if (filter == NULL) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if (!(filter->pole > 0.0f && filter->pole < 1.0f)) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    filter->previous_input = 0.0f;
    filter->previous_output = 0.0f;
    filter->initialized = false;
    return FILTER_STATUS_OK;
}

filter_status_t dc_blocker_process_sample(dc_blocker_t *filter,
                                          float input,
                                          float *output)
{
    float current_output;

    if ((filter == NULL) || (output == NULL)) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if (!(filter->pole > 0.0f && filter->pole < 1.0f)) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    if (!filter->initialized) {
        filter->previous_input = input;
        filter->previous_output = 0.0f;
        filter->initialized = true;
        *output = 0.0f;
        return FILTER_STATUS_OK;
    }

    current_output = filter->pole *
                     (filter->previous_output + input - filter->previous_input);
    filter->previous_input = input;
    filter->previous_output = current_output;
    *output = current_output;
    return FILTER_STATUS_OK;
}

filter_status_t dc_blocker_process_block(dc_blocker_t *filter,
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
        status = dc_blocker_process_sample(filter, input[i], &output[i]);
        if (status != FILTER_STATUS_OK) {
            return status;
        }
    }

    return FILTER_STATUS_OK;
}

