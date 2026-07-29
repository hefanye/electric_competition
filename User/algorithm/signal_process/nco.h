/**
 * @file nco.h
 * @brief 软件数控振荡器（NCO），产生正弦或正交 I/Q 参考信号。
 */

#ifndef NCO_H
#define NCO_H

#include <stddef.h>
#include "signal_process_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float sample_rate_hz;
    float frequency_hz;
    float phase_radians;
    float phase_step_radians;
    float amplitude;
    float offset;
} nco_f32_t;

/** 初始化 NCO，初始幅度为 1、偏置为 0、相位为 0。 */
signal_process_status_t nco_init_f32(
    nco_f32_t *nco,
    float sample_rate_hz,
    float frequency_hz);

/** 修改频率并重新计算相位步进，不会清零当前相位。 */
signal_process_status_t nco_set_frequency_f32(
    nco_f32_t *nco,
    float frequency_hz);

/** 修改输出幅度和直流偏置。 */
signal_process_status_t nco_set_amplitude_offset_f32(
    nco_f32_t *nco,
    float amplitude,
    float offset);

/** 设置当前相位；函数会自动归一化到 [0, 2*pi)。 */
signal_process_status_t nco_set_phase_f32(nco_f32_t *nco, float phase_radians);

/** 产生一个正弦样本并推进相位。 */
signal_process_status_t nco_next_sine_f32(nco_f32_t *nco, float *sample);

/** 产生同相 cos 和正交 sin 参考样本并推进相位。 */
signal_process_status_t nco_next_iq_f32(
    nco_f32_t *nco,
    float *in_phase,
    float *quadrature);

/** 连续产生一块正弦数据。 */
signal_process_status_t nco_generate_sine_f32(
    nco_f32_t *nco,
    float *output,
    size_t length);

#ifdef __cplusplus
}
#endif

#endif
