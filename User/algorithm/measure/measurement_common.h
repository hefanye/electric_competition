/**
 * @file measurement_common.h
 * @brief 通用测量算法库的公共类型、状态码和结果标志。
 *
 * 本文件仅依赖标准 C 头文件，不包含任何 MCU、SDK 或操作系统接口。
 */

#ifndef MEASUREMENT_COMMON_H
#define MEASUREMENT_COMMON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEASUREMENT_PI_F (3.14159265358979323846f)
#define MEASUREMENT_SQRT_TWO_F (1.41421356237309504880f)

/** 测量算法统一返回状态。 */
typedef enum {
    MEASUREMENT_STATUS_OK = 0,
    MEASUREMENT_STATUS_NULL_POINTER = -1,
    MEASUREMENT_STATUS_INVALID_PARAM = -2,
    MEASUREMENT_STATUS_INSUFFICIENT_DATA = -3,
    MEASUREMENT_STATUS_NO_SIGNAL = -4,
    MEASUREMENT_STATUS_NUMERIC_ERROR = -5
} measurement_status_t;

/** 测量结果状态标志，可以按位组合。 */
typedef enum {
    MEASUREMENT_FLAG_NONE = 0U,
    MEASUREMENT_FLAG_VALID = 1U << 0,
    MEASUREMENT_FLAG_LOW_SIGNAL = 1U << 1,
    MEASUREMENT_FLAG_CLIPPED = 1U << 2,
    MEASUREMENT_FLAG_UNSTABLE = 1U << 3,
    MEASUREMENT_FLAG_INVALID_SAMPLE = 1U << 4
} measurement_flag_t;

/** 通用单值测量结果。 */
typedef struct {
    float value;         /**< 测量值，单位由具体函数定义。 */
    float quality;       /**< 质量指标，通常限制在 0～1。 */
    uint32_t flags;      /**< measurement_flag_t 的按位组合。 */
    size_t samples_used; /**< 实际参与计算的样本数或周期数。 */
} measurement_result_t;

#ifdef __cplusplus
}
#endif

#endif /* MEASUREMENT_COMMON_H */

