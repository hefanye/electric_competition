/**
 * @file nco.c
 * @brief 基于浮点相位累加器的软件 NCO。
 */

#include "nco.h"

#include <math.h>

static float nco_wrap_phase(float phase)
{
    phase = fmodf(phase, SP_TWO_PI_F);
    if (phase < 0.0f) {
        phase += SP_TWO_PI_F;
    }
    return phase;
}

static void nco_advance_phase(nco_f32_t *nco)
{
    nco->phase_radians += nco->phase_step_radians;
    if (nco->phase_radians >= SP_TWO_PI_F || nco->phase_radians < 0.0f) {
        nco->phase_radians = nco_wrap_phase(nco->phase_radians);
    }
}

signal_process_status_t nco_init_f32(
    nco_f32_t *nco,
    float sample_rate_hz,
    float frequency_hz)
{
    if (nco == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (sample_rate_hz <= 0.0f || frequency_hz < 0.0f ||
        frequency_hz >= 0.5f * sample_rate_hz) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    nco->sample_rate_hz = sample_rate_hz;
    nco->frequency_hz = frequency_hz;
    nco->phase_radians = 0.0f;
    nco->phase_step_radians = SP_TWO_PI_F * frequency_hz / sample_rate_hz;
    nco->amplitude = 1.0f;
    nco->offset = 0.0f;
    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t nco_set_frequency_f32(
    nco_f32_t *nco,
    float frequency_hz)
{
    if (nco == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (nco->sample_rate_hz <= 0.0f || frequency_hz < 0.0f ||
        frequency_hz >= 0.5f * nco->sample_rate_hz) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    nco->frequency_hz = frequency_hz;
    nco->phase_step_radians = SP_TWO_PI_F * frequency_hz
                            / nco->sample_rate_hz;
    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t nco_set_amplitude_offset_f32(
    nco_f32_t *nco,
    float amplitude,
    float offset)
{
    if (nco == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (amplitude < 0.0f) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    nco->amplitude = amplitude;
    nco->offset = offset;
    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t nco_set_phase_f32(nco_f32_t *nco, float phase_radians)
{
    if (nco == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (!isfinite(phase_radians)) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    nco->phase_radians = nco_wrap_phase(phase_radians);
    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t nco_next_sine_f32(nco_f32_t *nco, float *sample)
{
    if (nco == NULL || sample == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }

    *sample = nco->offset + nco->amplitude * sinf(nco->phase_radians);
    nco_advance_phase(nco);
    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t nco_next_iq_f32(
    nco_f32_t *nco,
    float *in_phase,
    float *quadrature)
{
    if (nco == NULL || in_phase == NULL || quadrature == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }

    *in_phase = nco->offset + nco->amplitude * cosf(nco->phase_radians);
    *quadrature = nco->offset + nco->amplitude * sinf(nco->phase_radians);
    nco_advance_phase(nco);
    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t nco_generate_sine_f32(
    nco_f32_t *nco,
    float *output,
    size_t length)
{
    size_t i;

    if (nco == NULL || output == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (length == 0U) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    for (i = 0U; i < length; ++i) {
        signal_process_status_t status = nco_next_sine_f32(nco, &output[i]);
        if (status != SIGNAL_PROCESS_STATUS_OK) {
            return status;
        }
    }
    return SIGNAL_PROCESS_STATUS_OK;
}
