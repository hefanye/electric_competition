/**
 * @file am_envelope.h
 * @brief AM 全波整流包络跟踪和调制度估算。
 */

#ifndef AM_ENVELOPE_H
#define AM_ENVELOPE_H

#include <stddef.h>
#include "signal_process_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float attack_alpha;
    float release_alpha;
    float envelope;
    int initialized;
} am_envelope_follower_t;

/**
 * 用时间常数初始化。时间常数为 0 表示立即跟随，对应 alpha=1。
 */
signal_process_status_t am_envelope_init_times_f32(
    am_envelope_follower_t *follower,
    float sample_rate_hz,
    float attack_time_seconds,
    float release_time_seconds);

/** 用直接给定的一阶滤波系数初始化，系数范围为 (0, 1]。 */
signal_process_status_t am_envelope_init_alpha_f32(
    am_envelope_follower_t *follower,
    float attack_alpha,
    float release_alpha);

/** 重置包络状态。initial_envelope 必须非负。 */
signal_process_status_t am_envelope_reset_f32(
    am_envelope_follower_t *follower,
    float initial_envelope);

/** 处理一个输入样本，内部先取绝对值再执行攻/释一阶平滑。 */
signal_process_status_t am_envelope_process_sample_f32(
    am_envelope_follower_t *follower,
    float input,
    float *envelope);

/** 连续处理一块输入数据。input 与 output 可以是同一块内存。 */
signal_process_status_t am_envelope_process_block_f32(
    am_envelope_follower_t *follower,
    const float *input,
    float *output,
    size_t length);

/**
 * 从已经提取的包络估算 AM 调制度 m=(max-min)/(max+min)。
 * minimum_sum 用来拒绝过小信号。
 */
signal_process_status_t am_modulation_index_f32(
    const float *envelope,
    size_t length,
    float minimum_sum,
    float *modulation_index,
    float *minimum_envelope,
    float *maximum_envelope);

#ifdef __cplusplus
}
#endif

#endif
