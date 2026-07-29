/**
 * @file median_filter.c
 * @brief 中值滤波器实现，使用小规模插入排序，不使用动态内存。
 */

#include "median_filter.h"

static void insertion_sort(float *values, size_t length)
{
    size_t i;

    for (i = 1U; i < length; ++i) {
        float key = values[i];
        size_t j = i;

        while ((j > 0U) && (values[j - 1U] > key)) {
            values[j] = values[j - 1U];
            --j;
        }
        values[j] = key;
    }
}

float median_filter_3(float a, float b, float c)
{
    if (a > b) {
        float temp = a;
        a = b;
        b = temp;
    }
    if (b > c) {
        float temp = b;
        b = c;
        c = temp;
    }
    if (a > b) {
        b = a;
    }
    return b;
}

float median_filter_5(float a, float b, float c, float d, float e)
{
    float values[5];

    values[0] = a;
    values[1] = b;
    values[2] = c;
    values[3] = d;
    values[4] = e;
    insertion_sort(values, 5U);
    return values[2];
}

filter_status_t median_filter_init(median_filter_t *filter,
                                   float *sample_buffer,
                                   float *scratch_buffer,
                                   size_t window_size)
{
    size_t i;

    if ((filter == NULL) || (sample_buffer == NULL) || (scratch_buffer == NULL)) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if ((window_size < 3U) || ((window_size & 1U) == 0U)) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    filter->buffer = sample_buffer;
    filter->scratch = scratch_buffer;
    filter->window_size = window_size;
    filter->index = 0U;
    filter->count = 0U;

    for (i = 0U; i < window_size; ++i) {
        sample_buffer[i] = 0.0f;
        scratch_buffer[i] = 0.0f;
    }

    return FILTER_STATUS_OK;
}

filter_status_t median_filter_reset(median_filter_t *filter)
{
    size_t i;

    if ((filter == NULL) || (filter->buffer == NULL) || (filter->scratch == NULL)) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if ((filter->window_size < 3U) || ((filter->window_size & 1U) == 0U)) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    for (i = 0U; i < filter->window_size; ++i) {
        filter->buffer[i] = 0.0f;
        filter->scratch[i] = 0.0f;
    }
    filter->index = 0U;
    filter->count = 0U;

    return FILTER_STATUS_OK;
}

filter_status_t median_filter_process_sample(median_filter_t *filter,
                                             float input,
                                             float *output)
{
    size_t i;

    if ((filter == NULL) || (filter->buffer == NULL) ||
        (filter->scratch == NULL) || (output == NULL)) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if ((filter->window_size < 3U) || ((filter->window_size & 1U) == 0U)) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    filter->buffer[filter->index] = input;
    filter->index++;
    if (filter->index >= filter->window_size) {
        filter->index = 0U;
    }
    if (filter->count < filter->window_size) {
        filter->count++;
    }

    for (i = 0U; i < filter->count; ++i) {
        filter->scratch[i] = filter->buffer[i];
    }
    insertion_sort(filter->scratch, filter->count);

    if ((filter->count & 1U) != 0U) {
        *output = filter->scratch[filter->count / 2U];
    } else {
        const size_t upper = filter->count / 2U;
        *output = 0.5f * (filter->scratch[upper - 1U] + filter->scratch[upper]);
    }

    return FILTER_STATUS_OK;
}

filter_status_t median_filter_process_block(median_filter_t *filter,
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
        status = median_filter_process_sample(filter, input[i], &output[i]);
        if (status != FILTER_STATUS_OK) {
            return status;
        }
    }

    return FILTER_STATUS_OK;
}

