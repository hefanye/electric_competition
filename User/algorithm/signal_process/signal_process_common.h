/**
 * @file signal_process_common.h
 * @brief 信号处理算法库的公共类型、状态码和数学常量。
 *
 * 本库只依赖 C99 标准库，不分配动态内存，也不包含任何 MCU 外设代码。
 */

#ifndef SIGNAL_PROCESS_COMMON_H
#define SIGNAL_PROCESS_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/** 所有模块共用的返回状态。 */
typedef enum {
    SIGNAL_PROCESS_STATUS_OK = 0,
    SIGNAL_PROCESS_STATUS_COMPLETE = 1,
    SIGNAL_PROCESS_STATUS_NULL_POINTER = -1,
    SIGNAL_PROCESS_STATUS_INVALID_PARAM = -2,
    SIGNAL_PROCESS_STATUS_INSUFFICIENT_DATA = -3,
    SIGNAL_PROCESS_STATUS_NO_SIGNAL = -4,
    SIGNAL_PROCESS_STATUS_NUMERIC_ERROR = -5
} signal_process_status_t;

/** 单精度复数。 */
typedef struct {
    float real;
    float imag;
} sp_complex_f32_t;

#define SP_PI_F       3.14159265358979323846f
#define SP_TWO_PI_F   6.28318530717958647692f
#define SP_SQRT2_F    1.41421356237309504880f

#ifdef __cplusplus
}
#endif

#endif
