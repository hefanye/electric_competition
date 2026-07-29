/**
 * @file sweep_planner.c
 * @brief 线性等间隔与对数等比扫频的实现。
 */

#include "sweep_planner.h"

#include <math.h>

static int sweep_mode_is_valid(sweep_mode_t mode)
{
    return mode == SWEEP_MODE_LINEAR || mode == SWEEP_MODE_LOGARITHMIC;
}

signal_process_status_t sweep_planner_init(
    sweep_planner_t *planner,
    float start_frequency_hz,
    float stop_frequency_hz,
    size_t point_count,
    sweep_mode_t mode)
{
    if (planner == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (!sweep_mode_is_valid(mode) || point_count < 2U ||
        start_frequency_hz < 0.0f || stop_frequency_hz < 0.0f ||
        start_frequency_hz == stop_frequency_hz ||
        (mode == SWEEP_MODE_LOGARITHMIC &&
         (start_frequency_hz <= 0.0f || stop_frequency_hz <= 0.0f))) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    planner->start_frequency_hz = start_frequency_hz;
    planner->stop_frequency_hz = stop_frequency_hz;
    planner->point_count = point_count;
    planner->next_index = 0U;
    planner->mode = mode;
    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t sweep_planner_reset(sweep_planner_t *planner)
{
    if (planner == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    planner->next_index = 0U;
    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t sweep_planner_frequency_at(
    const sweep_planner_t *planner,
    size_t index,
    float *frequency_hz)
{
    float fraction;

    if (planner == NULL || frequency_hz == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (!sweep_mode_is_valid(planner->mode) || planner->point_count < 2U ||
        index >= planner->point_count) {
        return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
    }

    fraction = (float)index / (float)(planner->point_count - 1U);
    if (planner->mode == SWEEP_MODE_LINEAR) {
        *frequency_hz = planner->start_frequency_hz
                      + (planner->stop_frequency_hz
                         - planner->start_frequency_hz) * fraction;
    } else {
        if (planner->start_frequency_hz <= 0.0f ||
            planner->stop_frequency_hz <= 0.0f) {
            return SIGNAL_PROCESS_STATUS_INVALID_PARAM;
        }
        *frequency_hz = planner->start_frequency_hz *
            powf(planner->stop_frequency_hz / planner->start_frequency_hz,
                 fraction);
    }
    return SIGNAL_PROCESS_STATUS_OK;
}

signal_process_status_t sweep_planner_next(
    sweep_planner_t *planner,
    float *frequency_hz)
{
    signal_process_status_t status;

    if (planner == NULL || frequency_hz == NULL) {
        return SIGNAL_PROCESS_STATUS_NULL_POINTER;
    }
    if (planner->next_index >= planner->point_count) {
        return SIGNAL_PROCESS_STATUS_COMPLETE;
    }

    status = sweep_planner_frequency_at(
        planner, planner->next_index, frequency_hz);
    if (status == SIGNAL_PROCESS_STATUS_OK) {
        ++planner->next_index;
    }
    return status;
}
