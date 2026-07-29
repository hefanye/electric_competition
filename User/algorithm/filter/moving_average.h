/**
 * @file moving_average.h
 * @brief 流式滑动平均滤波器。
 *
 * 采用环形缓冲区和运行和，每个样本的计算复杂度为 O(1)。
 * 适用于直流量、缓慢变化量以及多次测量结果的平滑。
 * 移动平均会改变交流信号的幅值和相位，不应默认用于 FFT 或相位测量前。
 */

#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include "filter_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 滑动平均滤波器状态。工作缓冲区由调用方提供。 */
typedef struct {
    float *buffer;       /**< 长度至少为 window_size 的环形缓冲区。 */
    size_t window_size;  /**< 滑动窗口长度。 */
    size_t index;        /**< 下一个被替换的样本位置。 */
    size_t count;        /**< 已接收样本数，最大为 window_size。 */
    float sum;           /**< 当前有效窗口的样本和。 */
} moving_average_t;

/**
 * @brief 初始化滑动平均滤波器。
 * @param filter 滤波器状态。
 * @param work_buffer 调用方提供的工作缓冲区。
 * @param window_size 窗口长度，必须大于 0。
 */
filter_status_t moving_average_init(moving_average_t *filter,
                                    float *work_buffer,
                                    size_t window_size);

/** @brief 清空历史样本，保留原窗口配置。 */
filter_status_t moving_average_reset(moving_average_t *filter);

/**
 * @brief 输入一个新样本并返回当前滑动平均值。
 *
 * 窗口尚未填满时，输出已有样本的平均值，不使用零填充。
 */
filter_status_t moving_average_process_sample(moving_average_t *filter,
                                              float input,
                                              float *output);

/**
 * @brief 连续处理一块数据。
 * @note input 与 output 可以指向同一数组。
 */
filter_status_t moving_average_process_block(moving_average_t *filter,
                                             const float *input,
                                             float *output,
                                             size_t length);

#ifdef __cplusplus
}
#endif

#endif /* MOVING_AVERAGE_H */

