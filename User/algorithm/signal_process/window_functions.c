/**
 * @file window_functions.c
 * @brief 常用频谱分析窗函数的实现。
 */

#include "window_functions.h"

#include <math.h>

static int sp_window_type_is_valid(sp_window_type_t type)
{
    return type >= SP_WINDOW_RECTANGULAR && type <= SP_WINDOW_FLAT_TOP;
}

signal_process_status_t sp_window_coefficient_f32(
    sp_window_type_t type,
    size_t index,
    size_t length,
    float *coefficient)
{
    float angle;

    if (coefficient == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (!sp_window_type_is_valid(type) || length == 0U || index >= length) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    /* 单点数据没有“窗边缘”，约定所有窗的系数均为 1。 */
    if (length == 1U || type == SP_WINDOW_RECTANGULAR) {
        *coefficient = 1.0f;
        return SIGNAL_PROCESS_STATUS_OK;
    }

    angle = SP_TWO_PI_F * (float)index / (float)(length - 1U);
    switch (type) {
        case SP_WINDOW_HANN:
            *coefficient = 0.5f - 0.5f * cosf(angle);
            break;
        case SP_WINDOW_HAMMING:
            *coefficient = 0.54f - 0.46f * cosf(angle);
            break;
        case SP_WINDOW_BLACKMAN:
            *coefficient = 0.42f - 0.5f * cosf(angle)
                         + 0.08f * cosf(2.0f * angle);
            break;
        case SP_WINDOW_FLAT_TOP:
            *coefficient = 0.21557895f
                         - 0.41663158f * cosf(angle)
                         + 0.277263158f * cosf(2.0f * angle)
                         - 0.083578947f * cosf(3.0f * angle)
                         + 0.006947368f * cosf(4.0f * angle);
            break;
        default:
            return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t sp_window_apply_f32(
    const float *input,
    float *output,
    size_t length,
    sp_window_type_t type)
{
    size_t i;

    if (input == NULL || output == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (length == 0U || !sp_window_type_is_valid(type)) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    for (i = 0U; i < length; ++i) {
        float coefficient;
        signal_process_status_t status =
            sp_window_coefficient_f32(type, i, length, &coefficient);
        if (status != SIGNAL_PROCESS_STATUS_OK) {
            return status;
        }
        output[i] = input[i] * coefficient;
    }

    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t sp_window_coherent_gain_f32(
    sp_window_type_t type,
    size_t length,
    float *gain)
{
    size_t i;
    double sum = 0.0;

    if (gain == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (length == 0U || !sp_window_type_is_valid(type)) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    for (i = 0U; i < length; ++i) {
        float coefficient;
        signal_process_status_t status =
            sp_window_coefficient_f32(type, i, length, &coefficient);
        if (status != SIGNAL_PROCESS_STATUS_OK) {
            return status;
        }
        sum += (double)coefficient;
    }

    *gain = (float)(sum / (double)length);
    return (*gain > 0.0f) ? SIGNAL_PROCESS_STATUS_OK
                          : SIGNAL_PROCESS_STATUS_NUMERIC_ERROR;
}
