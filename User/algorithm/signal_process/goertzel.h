/**
 * @file goertzel.h
 * @brief 单个指定频率的幅值、功率和相位测量。
 */

#ifndef GOERTZEL_H
#define GOERTZEL_H

#include <stddef.h>
#include "window_functions.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float sample_rate_hz;
    float target_frequency_hz;
    sp_window_type_t window;
    int remove_mean;
} goertzel_config_t;

typedef struct {
    float real;
    float imag;
    float amplitude_peak;
    float power;
    float phase_radians;
} goertzel_result_t;

/**
 * 测量任意目标频率，不要求目标频率恰好落在 DFT 整数频点上。
 * power 定义为 amplitude_peak 的平方，便于不同频率间进行能量比较。
 */
signal_process_status_t goertzel_analyze_f32(
    const float *input,
    size_t length,
    const goertzel_config_t *config,
    goertzel_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
