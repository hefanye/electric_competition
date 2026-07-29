/**
 * @file moving_average.c
 * @brief 滑动平均滤波器实现。
 */

#include "moving_average.h"

filter_status_t moving_average_init(moving_average_t *filter,
                                    float *work_buffer,
                                    size_t window_size)
{
    size_t i;

    if ((filter == NULL) || (work_buffer == NULL)) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if (window_size == 0U) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    filter->buffer = work_buffer;
    filter->window_size = window_size;
    filter->index = 0U;
    filter->count = 0U;
    filter->sum = 0.0f;

    for (i = 0U; i < window_size; ++i) {
        work_buffer[i] = 0.0f;
    }

    return FILTER_STATUS_OK;
}

filter_status_t moving_average_reset(moving_average_t *filter)
{
    size_t i;

    if ((filter == NULL) || (filter->buffer == NULL)) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if (filter->window_size == 0U) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    for (i = 0U; i < filter->window_size; ++i) {
        filter->buffer[i] = 0.0f;
    }
    filter->index = 0U;
    filter->count = 0U;
    filter->sum = 0.0f;

    return FILTER_STATUS_OK;
}

filter_status_t moving_average_process_sample(moving_average_t *filter,
                                              float input,
                                              float *output)
{
    if ((filter == NULL) || (filter->buffer == NULL) || (output == NULL)) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if (filter->window_size == 0U) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    if (filter->count < filter->window_size) {
        filter->buffer[filter->index] = input;
        filter->sum += input;
        filter->count++;
    } else {
        filter->sum -= filter->buffer[filter->index];
        filter->buffer[filter->index] = input;
        filter->sum += input;
    }

    filter->index++;
    if (filter->index >= filter->window_size) {
        filter->index = 0U;
    }

    *output = filter->sum / (float)filter->count;
    return FILTER_STATUS_OK;
}

filter_status_t moving_average_process_block(moving_average_t *filter,
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
        status = moving_average_process_sample(filter, input[i], &output[i]);
        if (status != FILTER_STATUS_OK) {
            return status;
        }
    }

    return FILTER_STATUS_OK;
}

