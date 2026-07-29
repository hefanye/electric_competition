/**
 * @file sweep_planner.h
 * @brief 线性和对数扫频点规划器，不包含任何 DDS/定时器驱动。
 */

#ifndef SWEEP_PLANNER_H
#define SWEEP_PLANNER_H

#include <stddef.h>
#include "signal_process_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SWEEP_MODE_LINEAR = 0,
    SWEEP_MODE_LOGARITHMIC
} sweep_mode_t;

typedef struct {
    float start_frequency_hz;
    float stop_frequency_hz;
    size_t point_count;
    size_t next_index;
    sweep_mode_t mode;
} sweep_planner_t;

/** 初始化扫频规划器。point_count 至少为 2。 */
signal_process_status_t sweep_planner_init(
    sweep_planner_t *planner,
    float start_frequency_hz,
    float stop_frequency_hz,
    size_t point_count,
    sweep_mode_t mode);

/** 重新从第 0 个扫频点开始。 */
signal_process_status_t sweep_planner_reset(sweep_planner_t *planner);

/** 按索引计算频点，但不改变 next_index。 */
signal_process_status_t sweep_planner_frequency_at(
    const sweep_planner_t *planner,
    size_t index,
    float *frequency_hz);

/**
 * 取出下一个频点；全部取完后返回 SIGNAL_PROCESS_STATUS_COMPLETE。
 */
signal_process_status_t sweep_planner_next(
    sweep_planner_t *planner,
    float *frequency_hz);

#ifdef __cplusplus
}
#endif

#endif
