/**
 * @file fft.h
 * @brief 原地基 2 FFT 和实数输入 FFT。
 */

#ifndef FFT_H
#define FFT_H

#include <stddef.h>
#include "signal_process_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 判断 length 是否为合法的 2 的整数次幂。 */
int fft_is_power_of_two(size_t length);

/**
 * 对复数数组执行原地 FFT。
 * inverse 为 0 时执行正变换，为非 0 时执行逆变换；逆变换内部除以 N。
 */
signal_process_status_t fft_transform_inplace_f32(
    sp_complex_f32_t *data,
    size_t length,
    int inverse);

/** 把实数输入复制到 output 后执行正向 FFT。output 至少有 length 个元素。 */
signal_process_status_t fft_real_forward_f32(
    const float *input,
    sp_complex_f32_t *output,
    size_t length);

#ifdef __cplusplus
}
#endif

#endif
