/**
 * @file goertzel.c
 * @brief 带可选去均值和窗函数幅值校正的 Goertzel 算法。
 */

#include "goertzel.h"

#include <math.h>

signal_process_status_t goertzel_analyze_f32(
    const float *input,
    size_t length,
    const goertzel_config_t *config,
    goertzel_result_t *result)
{
    size_t i;
    double mean = 0.0;
    double q1 = 0.0;
    double q2 = 0.0;
    double window_sum = 0.0;
    double real;
    double imag;
    double angular_frequency;
    double cosine;
    double sine;
    double recurrence_coefficient;
    double phase;

    if (input == NULL || config == NULL || result == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (length < 2U || config->sample_rate_hz <= 0.0f ||
        config->target_frequency_hz <= 0.0f ||
        config->target_frequency_hz >= 0.5f * config->sample_rate_hz) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    angular_frequency = 2.0 * (double)SP_PI_F
                      * (double)config->target_frequency_hz
                      / (double)config->sample_rate_hz;
    cosine = cos(angular_frequency);
    sine = sin(angular_frequency);
    recurrence_coefficient = 2.0 * cosine;

    if (config->remove_mean) {
        for (i = 0U; i < length; ++i) {
            mean += (double)input[i];
        }
        mean /= (double)length;
    }

    for (i = 0U; i < length; ++i) {
        float window_coefficient;
        double q0;
        signal_process_status_t status = sp_window_coefficient_f32(
            config->window, i, length, &window_coefficient);
        if (status != SIGNAL_PROCESS_STATUS_OK) {
            return status;
        }

        q0 = ((double)input[i] - mean) * (double)window_coefficient
           + recurrence_coefficient * q1 - q2;
        q2 = q1;
        q1 = q0;
        window_sum += (double)window_coefficient;
    }

    if (window_sum <= 0.0) {
        return SIGNAL_PROCESS_STATUS_NUMERIC_ERROR;
    }

    real = q1 - q2 * cosine;
    imag = q2 * sine;
    result->real = (float)real;
    result->imag = (float)imag;
    result->amplitude_peak =
        (float)(2.0 * sqrt(real * real + imag * imag) / window_sum);
    result->power = result->amplitude_peak * result->amplitude_peak;
    /*
     * 递推终值的相位参考位于第 N-1 个样本，减去该旋转后，返回值
     * 与 x[n] = A*cos(w*n + phase) 中以 n=0 为参考的 phase 一致。
     */
    phase = atan2(imag, real)
          - angular_frequency * (double)(length - 1U);
    phase = remainder(phase, (double)SP_TWO_PI_F);
    result->phase_radians = (float)phase;
    return SIGNAL_PROCESS_STATUS_OK;
}
