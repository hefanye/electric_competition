/**
 * @file calibration.h
 * @brief 标量测量值的线性、多点拟合和分段线性校准。
 */

#ifndef MEASUREMENT_CALIBRATION_H
#define MEASUREMENT_CALIBRATION_H

#include <stdbool.h>

#include "measurement_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** y = gain*x + offset 线性校准模型。 */
typedef struct {
    float gain;
    float offset;
    float rmse; /**< 多点拟合均方根误差；两点校准时为 0。 */
} linear_calibration_t;

/** @brief 使用两个不同原始值生成精确线性校准模型。 */
measurement_status_t calibration_linear_from_two_points(
    float raw_1,
    float reference_1,
    float raw_2,
    float reference_2,
    linear_calibration_t *calibration);

/** @brief 使用最小二乘法拟合多点线性校准模型。 */
measurement_status_t calibration_linear_fit_f32(
    const float *raw_values,
    const float *reference_values,
    size_t length,
    linear_calibration_t *calibration);

/** @brief 应用 y = gain*x + offset。 */
measurement_status_t calibration_linear_apply(
    const linear_calibration_t *calibration,
    float raw_value,
    float *corrected_value);

/**
 * @brief 应用分段线性校准表。
 *
 * raw_points 必须严格递增。输入超出表格时，allow_extrapolation=false 会钳位
 * 到端点参考值，true 则使用首段或末段直线外推。
 */
measurement_status_t calibration_piecewise_apply_f32(
    const float *raw_points,
    const float *reference_points,
    size_t point_count,
    float raw_value,
    bool allow_extrapolation,
    float *corrected_value);

#ifdef __cplusplus
}
#endif

#endif /* MEASUREMENT_CALIBRATION_H */

