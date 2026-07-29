/**
 * @file stability_detector.h
 * @brief 测量结果的流式稳定性判断。
 *
 * 该模块处理的是“多次测量结果”，不是原始高速波形。调用方提供窗口缓冲区，
 * 当窗口内极差小于绝对或相对容差时判定稳定。
 */

#ifndef STABILITY_DETECTOR_H
#define STABILITY_DETECTOR_H

#include <stdbool.h>

#include "measurement_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 稳定性检测器状态。 */
typedef struct {
    float *buffer;
    size_t window_size;
    size_t index;
    size_t count;
    float absolute_tolerance;
    float relative_tolerance;
} stability_detector_t;

/** 当前稳定性分析结果。 */
typedef struct {
    float mean;
    float minimum;
    float maximum;
    float standard_deviation;
    float allowed_span;
    float actual_span;
    float quality;
    size_t sample_count;
    bool stable;
} stability_result_t;

/**
 * @brief 初始化稳定性检测器。
 * @param absolute_tolerance 允许的最小绝对极差。
 * @param relative_tolerance 相对于 abs(mean) 的允许极差比例。
 */
measurement_status_t stability_detector_init(stability_detector_t *detector,
                                             float *work_buffer,
                                             size_t window_size,
                                             float absolute_tolerance,
                                             float relative_tolerance);

/** @brief 清除历史测量值，保留窗口和容差配置。 */
measurement_status_t stability_detector_reset(stability_detector_t *detector);

/** @brief 加入一个新的测量值并返回当前稳定性。 */
measurement_status_t stability_detector_process(stability_detector_t *detector,
                                                float value,
                                                stability_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* STABILITY_DETECTOR_H */

