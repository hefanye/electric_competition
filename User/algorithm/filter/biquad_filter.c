/**
 * @file biquad_filter.c
 * @brief 通用 Biquad 滤波器实现。
 *
 * 系数设计采用 RBJ Audio EQ Cookbook 中的标准二阶形式，适用于通用数字
 * 低通、高通、带通和陷波。运行核心使用 Direct Form I。
 */

#include "biquad_filter.h"

#include <math.h>

#define BUTTERWORTH_Q_F (0.70710678118654752440f)

static filter_status_t validate_design_parameters(float sample_rate,
                                                  float frequency,
                                                  float q)
{
    if (!(sample_rate > 0.0f) || !(frequency > 0.0f) ||
        !(frequency < 0.5f * sample_rate) || !(q > 0.0f)) {
        return FILTER_STATUS_INVALID_PARAM;
    }
    return FILTER_STATUS_OK;
}

static void normalize_coefficients(biquad_coefficients_t *coefficients,
                                   float b0,
                                   float b1,
                                   float b2,
                                   float a0,
                                   float a1,
                                   float a2)
{
    const float inverse_a0 = 1.0f / a0;

    coefficients->b0 = b0 * inverse_a0;
    coefficients->b1 = b1 * inverse_a0;
    coefficients->b2 = b2 * inverse_a0;
    coefficients->a1 = a1 * inverse_a0;
    coefficients->a2 = a2 * inverse_a0;
}

filter_status_t biquad_filter_init(biquad_filter_t *filter,
                                   const biquad_coefficients_t *coefficients)
{
    if ((filter == NULL) || (coefficients == NULL)) {
        return FILTER_STATUS_NULL_POINTER;
    }

    filter->coefficients = *coefficients;
    filter->x1 = 0.0f;
    filter->x2 = 0.0f;
    filter->y1 = 0.0f;
    filter->y2 = 0.0f;
    return FILTER_STATUS_OK;
}

filter_status_t biquad_filter_reset(biquad_filter_t *filter)
{
    if (filter == NULL) {
        return FILTER_STATUS_NULL_POINTER;
    }

    filter->x1 = 0.0f;
    filter->x2 = 0.0f;
    filter->y1 = 0.0f;
    filter->y2 = 0.0f;
    return FILTER_STATUS_OK;
}

filter_status_t biquad_filter_set_coefficients(
    biquad_filter_t *filter,
    const biquad_coefficients_t *coefficients,
    int reset_state)
{
    if ((filter == NULL) || (coefficients == NULL)) {
        return FILTER_STATUS_NULL_POINTER;
    }

    filter->coefficients = *coefficients;
    if (reset_state != 0) {
        return biquad_filter_reset(filter);
    }
    return FILTER_STATUS_OK;
}

filter_status_t biquad_filter_process_sample(biquad_filter_t *filter,
                                             float input,
                                             float *output)
{
    float y;
    const biquad_coefficients_t *c;

    if ((filter == NULL) || (output == NULL)) {
        return FILTER_STATUS_NULL_POINTER;
    }

    c = &filter->coefficients;
    y = c->b0 * input + c->b1 * filter->x1 + c->b2 * filter->x2
        - c->a1 * filter->y1 - c->a2 * filter->y2;

    filter->x2 = filter->x1;
    filter->x1 = input;
    filter->y2 = filter->y1;
    filter->y1 = y;
    *output = y;
    return FILTER_STATUS_OK;
}

filter_status_t biquad_filter_process_block(biquad_filter_t *filter,
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
        status = biquad_filter_process_sample(filter, input[i], &output[i]);
        if (status != FILTER_STATUS_OK) {
            return status;
        }
    }

    return FILTER_STATUS_OK;
}

filter_status_t biquad_design_lowpass(biquad_coefficients_t *coefficients,
                                      float sample_rate,
                                      float cutoff_frequency,
                                      float q)
{
    float omega;
    float cosine;
    float alpha;

    if (coefficients == NULL) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if (validate_design_parameters(sample_rate, cutoff_frequency, q) !=
        FILTER_STATUS_OK) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    omega = 2.0f * FILTER_PI_F * cutoff_frequency / sample_rate;
    cosine = cosf(omega);
    alpha = sinf(omega) / (2.0f * q);

    normalize_coefficients(coefficients,
                           0.5f * (1.0f - cosine),
                           1.0f - cosine,
                           0.5f * (1.0f - cosine),
                           1.0f + alpha,
                           -2.0f * cosine,
                           1.0f - alpha);
    return FILTER_STATUS_OK;
}

filter_status_t biquad_design_highpass(biquad_coefficients_t *coefficients,
                                       float sample_rate,
                                       float cutoff_frequency,
                                       float q)
{
    float omega;
    float cosine;
    float alpha;

    if (coefficients == NULL) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if (validate_design_parameters(sample_rate, cutoff_frequency, q) !=
        FILTER_STATUS_OK) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    omega = 2.0f * FILTER_PI_F * cutoff_frequency / sample_rate;
    cosine = cosf(omega);
    alpha = sinf(omega) / (2.0f * q);

    normalize_coefficients(coefficients,
                           0.5f * (1.0f + cosine),
                           -(1.0f + cosine),
                           0.5f * (1.0f + cosine),
                           1.0f + alpha,
                           -2.0f * cosine,
                           1.0f - alpha);
    return FILTER_STATUS_OK;
}

filter_status_t biquad_design_bandpass(biquad_coefficients_t *coefficients,
                                       float sample_rate,
                                       float center_frequency,
                                       float q)
{
    float omega;
    float cosine;
    float alpha;

    if (coefficients == NULL) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if (validate_design_parameters(sample_rate, center_frequency, q) !=
        FILTER_STATUS_OK) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    omega = 2.0f * FILTER_PI_F * center_frequency / sample_rate;
    cosine = cosf(omega);
    alpha = sinf(omega) / (2.0f * q);

    normalize_coefficients(coefficients,
                           alpha,
                           0.0f,
                           -alpha,
                           1.0f + alpha,
                           -2.0f * cosine,
                           1.0f - alpha);
    return FILTER_STATUS_OK;
}

filter_status_t biquad_design_notch(biquad_coefficients_t *coefficients,
                                    float sample_rate,
                                    float notch_frequency,
                                    float q)
{
    float omega;
    float cosine;
    float alpha;

    if (coefficients == NULL) {
        return FILTER_STATUS_NULL_POINTER;
    }
    if (validate_design_parameters(sample_rate, notch_frequency, q) !=
        FILTER_STATUS_OK) {
        return FILTER_STATUS_INVALID_PARAM;
    }

    omega = 2.0f * FILTER_PI_F * notch_frequency / sample_rate;
    cosine = cosf(omega);
    alpha = sinf(omega) / (2.0f * q);

    normalize_coefficients(coefficients,
                           1.0f,
                           -2.0f * cosine,
                           1.0f,
                           1.0f + alpha,
                           -2.0f * cosine,
                           1.0f - alpha);
    return FILTER_STATUS_OK;
}

filter_status_t biquad_design_butterworth_lowpass(
    biquad_coefficients_t *coefficients,
    float sample_rate,
    float cutoff_frequency)
{
    return biquad_design_lowpass(coefficients,
                                 sample_rate,
                                 cutoff_frequency,
                                 BUTTERWORTH_Q_F);
}

filter_status_t biquad_design_butterworth_highpass(
    biquad_coefficients_t *coefficients,
    float sample_rate,
    float cutoff_frequency)
{
    return biquad_design_highpass(coefficients,
                                  sample_rate,
                                  cutoff_frequency,
                                  BUTTERWORTH_Q_F);
}

