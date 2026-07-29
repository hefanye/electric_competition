/**
 * @file filter_common.h
 * @brief 滤波算法库的公共类型、状态码和常量。
 *
 * 本文件不依赖任何 MCU、SDK 或操作系统，可用于标准 C99 环境。
 */

#ifndef FILTER_COMMON_H
#define FILTER_COMMON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 圆周率。避免依赖部分编译器未定义的 M_PI。 */
#define FILTER_PI_F (3.14159265358979323846f)

/** 滤波函数统一返回状态。 */
typedef enum {
    FILTER_STATUS_OK = 0,             /**< 调用成功。 */
    FILTER_STATUS_NULL_POINTER = -1,  /**< 输入、输出、状态或缓冲区为空。 */
    FILTER_STATUS_INVALID_PARAM = -2  /**< 窗长、采样率、频率或 Q 值非法。 */
} filter_status_t;

#ifdef __cplusplus
}
#endif

#endif /* FILTER_COMMON_H */

