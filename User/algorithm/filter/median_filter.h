/**
 * @file median_filter.h
 * @brief 中值滤波器。
 *
 * 中值滤波用于消除孤立的脉冲异常点。它属于非线性滤波，会改变波形，
 * 不应无条件用于频谱或相位分析。继电器切换后的完整暂态应直接丢弃，
 * 而不是依靠中值滤波掩盖。
 */

#ifndef MEDIAN_FILTER_H
#define MEDIAN_FILTER_H

#include "filter_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 通用流式中值滤波器状态。所有缓冲区均由调用方提供。 */
typedef struct {
    float *buffer;       /**< 环形样本缓冲区，长度为 window_size。 */
    float *scratch;      /**< 排序临时缓冲区，长度为 window_size。 */
    size_t window_size;  /**< 窗口长度，必须为大于等于 3 的奇数。 */
    size_t index;        /**< 下一个被替换的位置。 */
    size_t count;        /**< 当前有效样本数。 */
} median_filter_t;

/** @brief 计算三个数的中值，无状态、无工作缓冲区。 */
float median_filter_3(float a, float b, float c);

/** @brief 计算五个数的中值，无状态、无工作缓冲区。 */
float median_filter_5(float a, float b, float c, float d, float e);

/**
 * @brief 初始化通用流式中值滤波器。
 * @param sample_buffer 调用方提供的环形样本缓冲区。
 * @param scratch_buffer 调用方提供的排序缓冲区。
 * @param window_size 大于等于 3 的奇数，常用值为 3、5、7。
 */
filter_status_t median_filter_init(median_filter_t *filter,
                                   float *sample_buffer,
                                   float *scratch_buffer,
                                   size_t window_size);

/** @brief 清空历史样本，保留窗口和缓冲区配置。 */
filter_status_t median_filter_reset(median_filter_t *filter);

/**
 * @brief 输入一个样本并输出当前窗口中值。
 *
 * 启动阶段样本数为偶数时，输出排序后中间两个样本的平均值。
 */
filter_status_t median_filter_process_sample(median_filter_t *filter,
                                             float input,
                                             float *output);

/** @brief 连续处理一块数据，input 与 output 可以相同。 */
filter_status_t median_filter_process_block(median_filter_t *filter,
                                            const float *input,
                                            float *output,
                                            size_t length);

#ifdef __cplusplus
}
#endif

#endif /* MEDIAN_FILTER_H */

