/**
 * @file fft.c
 * @brief 无动态内存的迭代式基 2 Cooley-Tukey FFT。
 */

#include "fft.h"

#include <math.h>

int fft_is_power_of_two(size_t length)
{
    return length >= 2U && (length & (length - 1U)) == 0U;
}

static void fft_swap_complex(sp_complex_f32_t *a, sp_complex_f32_t *b)
{
    sp_complex_f32_t temporary = *a;
    *a = *b;
    *b = temporary;
}

signal_process_status_t fft_transform_inplace_f32(
    sp_complex_f32_t *data,
    size_t length,
    int inverse)
{
    size_t i;
    size_t j;
    size_t block_length;

    if (data == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (!fft_is_power_of_two(length)) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    /* 位倒序排列。 */
    j = 0U;
    for (i = 1U; i < length; ++i) {
        size_t bit = length >> 1U;
        while ((j & bit) != 0U) {
            j ^= bit;
            bit >>= 1U;
        }
        j ^= bit;
        if (i < j) {
            fft_swap_complex(&data[i], &data[j]);
        }
    }

    for (block_length = 2U; block_length <= length; block_length <<= 1U) {
        const float sign = inverse ? 1.0f : -1.0f;
        const float angle = sign * SP_TWO_PI_F / (float)block_length;
        const float root_real = cosf(angle);
        const float root_imag = sinf(angle);
        size_t block_start;

        for (block_start = 0U; block_start < length;
             block_start += block_length) {
            float twiddle_real = 1.0f;
            float twiddle_imag = 0.0f;
            size_t offset;

            for (offset = 0U; offset < block_length / 2U; ++offset) {
                sp_complex_f32_t *even = &data[block_start + offset];
                sp_complex_f32_t *odd =
                    &data[block_start + offset + block_length / 2U];
                const float odd_real = odd->real * twiddle_real
                                     - odd->imag * twiddle_imag;
                const float odd_imag = odd->real * twiddle_imag
                                     + odd->imag * twiddle_real;
                const float even_real = even->real;
                const float even_imag = even->imag;
                const float next_twiddle_real =
                    twiddle_real * root_real - twiddle_imag * root_imag;

                even->real = even_real + odd_real;
                even->imag = even_imag + odd_imag;
                odd->real = even_real - odd_real;
                odd->imag = even_imag - odd_imag;

                twiddle_imag = twiddle_real * root_imag
                             + twiddle_imag * root_real;
                twiddle_real = next_twiddle_real;
            }
        }

        /* 避免 size_t 左移溢出后形成死循环。 */
        if (block_length == length) {
            break;
        }
    }

    if (inverse) {
        const float scale = 1.0f / (float)length;
        for (i = 0U; i < length; ++i) {
            data[i].real *= scale;
            data[i].imag *= scale;
        }
    }

    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t fft_real_forward_f32(
    const float *input,
    sp_complex_f32_t *output,
    size_t length)
{
    size_t i;

    if (input == NULL || output == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (!fft_is_power_of_two(length)) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    for (i = 0U; i < length; ++i) {
        output[i].real = input[i];
        output[i].imag = 0.0f;
    }

    return fft_transform_inplace_f32(output, length, 0);
}
