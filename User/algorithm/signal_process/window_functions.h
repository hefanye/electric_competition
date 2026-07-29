/**
 * @file window_functions.h
 * @brief 矩形窗、Hann 窗、Hamming 窗、Blackman 窗和平顶窗。
 */

#ifndef WINDOW_FUNCTIONS_H
#define WINDOW_FUNCTIONS_H

#include <stddef.h>
#include "signal_process_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SP_WINDOW_RECTANGULAR = 0,
    SP_WINDOW_HANN,
    SP_WINDOW_HAMMING,
    SP_WINDOW_BLACKMAN,
    SP_WINDOW_FLAT_TOP
} sp_window_type_t;

/** 计算指定窗在 index 位置的系数。 */
signal_process_status_t sp_window_coefficient_f32(
    sp_window_type_t type,
    size_t index,
    size_t length,
    float *coefficient);

/** 对输入数据逐点加窗。input 与 output 可以指向同一块内存。 */
signal_process_status_t sp_window_apply_f32(
    const float *input,
    float *output,
    size_t length,
    sp_window_type_t type);

/**
 * 计算窗函数的相干增益：sum(window) / N。
 * 频谱幅值校正时必须除以该值。
 */
signal_process_status_t sp_window_coherent_gain_f32(
    sp_window_type_t type,
    size_t length,
    float *gain);

#ifdef __cplusplus
}
#endif

#endif
