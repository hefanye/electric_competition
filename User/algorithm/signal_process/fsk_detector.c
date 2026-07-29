/**
 * @file fsk_detector.c
 * @brief 双 Goertzel FSK 判决，含无信号和模糊信号状态。
 */

#include "fsk_detector.h"

#include <float.h>

signal_process_status_t fsk_detector_analyze_f32(
    const float *input,
    size_t length,
    const fsk_detector_config_t *config,
    fsk_detector_result_t *result)
{
    goertzel_config_t goertzel_config;
    goertzel_result_t tone_0;
    goertzel_result_t tone_1;
    float stronger_power;
    float weaker_power;
    signal_process_status_t status;

    if (input == NULL || config == NULL || result == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (length < 2U || config->sample_rate_hz <= 0.0f ||
        config->tone_0_hz <= 0.0f || config->tone_1_hz <= 0.0f ||
        config->tone_0_hz == config->tone_1_hz ||
        config->tone_0_hz >= 0.5f * config->sample_rate_hz ||
        config->tone_1_hz >= 0.5f * config->sample_rate_hz ||
        config->minimum_amplitude < 0.0f ||
        config->minimum_power_ratio < 1.0f) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    goertzel_config.sample_rate_hz = config->sample_rate_hz;
    goertzel_config.window = config->window;
    goertzel_config.remove_mean = config->remove_mean;
    goertzel_config.target_frequency_hz = config->tone_0_hz;
    status = goertzel_analyze_f32(input, length, &goertzel_config, &tone_0);
    if (status != SIGNAL_PROCESS_STATUS_OK) {
        return status;
    }

    goertzel_config.target_frequency_hz = config->tone_1_hz;
    status = goertzel_analyze_f32(input, length, &goertzel_config, &tone_1);
    if (status != SIGNAL_PROCESS_STATUS_OK) {
        return status;
    }

    result->tone_0_amplitude = tone_0.amplitude_peak;
    result->tone_1_amplitude = tone_1.amplitude_peak;
    result->tone_0_power = tone_0.power;
    result->tone_1_power = tone_1.power;

    if (tone_0.amplitude_peak < config->minimum_amplitude &&
        tone_1.amplitude_peak < config->minimum_amplitude) {
        result->decision = FSK_DECISION_NO_SIGNAL;
        result->power_ratio = 0.0f;
        result->confidence = 0.0f;
        return SIGNAL_PROCESS_STATUS_OK;
    }

    if (tone_0.power >= tone_1.power) {
        stronger_power = tone_0.power;
        weaker_power = tone_1.power;
    } else {
        stronger_power = tone_1.power;
        weaker_power = tone_0.power;
    }

    result->power_ratio = stronger_power / (weaker_power + FLT_MIN);
    result->confidence = (stronger_power - weaker_power)
                       / (stronger_power + weaker_power + FLT_MIN);

    if (result->power_ratio < config->minimum_power_ratio) {
        result->decision = FSK_DECISION_UNCERTAIN;
    } else {
        result->decision = tone_0.power >= tone_1.power
            ? FSK_DECISION_TONE_0
            : FSK_DECISION_TONE_1;
    }

    return SIGNAL_PROCESS_STATUS_OK;
}
