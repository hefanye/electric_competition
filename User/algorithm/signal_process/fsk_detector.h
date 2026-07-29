/**
 * @file fsk_detector.h
 * @brief 基于双 Goertzel 的二进制 FSK 单帧判决。
 */

#ifndef FSK_DETECTOR_H
#define FSK_DETECTOR_H

#include <stddef.h>
#include "goertzel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FSK_DECISION_NO_SIGNAL = 0,
    FSK_DECISION_TONE_0,
    FSK_DECISION_TONE_1,
    FSK_DECISION_UNCERTAIN
} fsk_decision_t;

typedef struct {
    float sample_rate_hz;
    float tone_0_hz;
    float tone_1_hz;
    sp_window_type_t window;
    int remove_mean;
    float minimum_amplitude;
    float minimum_power_ratio;
} fsk_detector_config_t;

typedef struct {
    fsk_decision_t decision;
    float tone_0_amplitude;
    float tone_1_amplitude;
    float tone_0_power;
    float tone_1_power;
    float power_ratio;
    float confidence; /**< (strong-weak)/(strong+weak)，范围约为 0~1。 */
} fsk_detector_result_t;

/** 对一个完整符号窗口作双频能量比较。 */
signal_process_status_t fsk_detector_analyze_f32(
    const float *input,
    size_t length,
    const fsk_detector_config_t *config,
    fsk_detector_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
