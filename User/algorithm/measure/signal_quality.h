/**
 * @file signal_quality.h
 * @brief 测量前的信号有效性、削顶和低幅度检查。
 */

#ifndef SIGNAL_QUALITY_H
#define SIGNAL_QUALITY_H

#include <stdbool.h>

#include "measurement_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 信号质量判断配置。 */
typedef struct {
    float input_low_limit;       /**< 输入允许范围下限。 */
    float input_high_limit;      /**< 输入允许范围上限。 */
    float clipping_margin;       /**< 距上下限小于该值时视为削顶。 */
    float min_ac_rms;            /**< 最小有效交流 RMS。 */
    float max_clipped_fraction;  /**< 最大允许削顶样本比例，范围 0～1。 */
} signal_quality_config_t;

/** 信号质量分析结果。 */
typedef struct {
    float minimum;
    float maximum;
    float mean;
    float ac_rms;
    float clipped_fraction;
    float quality;               /**< 0～1，主要反映有效样本和削顶比例。 */
    size_t clipped_count;
    size_t invalid_count;
    uint32_t flags;
    bool usable;                 /**< 无非法样本、不过度削顶且幅值足够。 */
} signal_quality_result_t;

/** @brief 分析一块采样数据是否适合继续进行测量。 */
measurement_status_t signal_quality_analyze_f32(
    const float *input,
    size_t length,
    const signal_quality_config_t *config,
    signal_quality_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* SIGNAL_QUALITY_H */

