/**
 * @file statistics.h
 * @brief 测量数据的基础统计函数。
 *
 * 提供均值、极值、总体/样本方差、标准差和去均值。所有函数均支持 float
 * 数据，不使用动态内存。
 */

#ifndef MEASUREMENT_STATISTICS_H
#define MEASUREMENT_STATISTICS_H

#include "measurement_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 计算数组算术平均值。 */
measurement_status_t statistics_mean_f32(const float *input,
                                         size_t length,
                                         float *mean);

/** @brief 同时计算最小值和最大值。 */
measurement_status_t statistics_min_max_f32(const float *input,
                                            size_t length,
                                            float *minimum,
                                            float *maximum);

/** @brief 计算总体方差，分母为 N。 */
measurement_status_t statistics_variance_population_f32(const float *input,
                                                        size_t length,
                                                        float *variance);

/** @brief 计算样本方差，分母为 N-1，至少需要两个样本。 */
measurement_status_t statistics_variance_sample_f32(const float *input,
                                                    size_t length,
                                                    float *variance);

/** @brief 计算总体标准差。 */
measurement_status_t statistics_stddev_population_f32(const float *input,
                                                      size_t length,
                                                      float *standard_deviation);

/**
 * @brief 从每个样本中减去数组均值。
 * @param removed_mean 可选；非空时返回被减去的均值。
 * @note input 与 output 可以指向同一数组。
 */
measurement_status_t statistics_remove_mean_f32(const float *input,
                                                float *output,
                                                size_t length,
                                                float *removed_mean);

#ifdef __cplusplus
}
#endif

#endif /* MEASUREMENT_STATISTICS_H */

